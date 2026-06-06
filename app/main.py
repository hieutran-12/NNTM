import os
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, validator, root_validator
from typing import Optional, List
from datetime import datetime, timedelta, timezone
try:
    from zoneinfo import ZoneInfo
except Exception:
    ZoneInfo = None
from bson import ObjectId
import motor.motor_asyncio

app = FastAPI(title="Soil Monitoring API")

# Allow CORS for frontend dev server
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


class SensorData(BaseModel):
    """Sensor payload. Numeric fields are Optional[float] and default to None.

    None means "no data" (not zero). This preserves the distinction between
    missing readings and actual zero values from sensors.
    """
    device_id: Optional[str] = "ESP8266"
    soil_moisture: Optional[float] = None
    light: Optional[float] = None
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    water_level: Optional[float] = None
    created_at: Optional[datetime] = None

    @root_validator(pre=True)
    def normalize_keys(cls, values):
        alias_map = {
            "soil_moisture": ["soil_moisture", "moisture"],
            "light": ["light"],
            "temperature": ["temperature", "temp"],
            "humidity": ["humidity", "hum"],
            "water_level": ["water_level", "waterLevel"],
        }
        for field, aliases in alias_map.items():
            for alias in aliases:
                if alias in values and values.get(alias) is not None:
                    values[field] = values.get(alias)
                    break
        return values

    @validator("device_id", pre=True, always=True)
    def default_device_id(cls, value):
        return value or "ESP8266"


class PumpCommand(BaseModel):
    device_id: str
    command: str  # e.g., "PUMP_ON" or "PUMP_OFF"
    duration: Optional[int] = None


# MongoDB client will be created on startup
mongo_client: Optional[motor.motor_asyncio.AsyncIOMotorClient] = None
db = None


@app.on_event("startup")
async def startup_db_client():
    global mongo_client, db
    mongo_uri = os.getenv(
        "MONGO_URI",
        "mongodb+srv://hieuminhtran101_db_user:rcpzv39sBjOF2zuA@nckh.uzxbcz8.mongodb.net/?appName=NCKH",
    )
    mongo_db_name = os.getenv("MONGO_DB_NAME", "NNTM")
    mongo_client = motor.motor_asyncio.AsyncIOMotorClient(mongo_uri)
    db = mongo_client.get_default_database(mongo_db_name)


@app.on_event("shutdown")
async def shutdown_db_client():
    global mongo_client
    if mongo_client:
        mongo_client.close()


@app.post("/api/sensor")
async def post_sensor(data: SensorData):
    """Receive sensor data from ESP32, store normalized data in MongoDB, and return pump decision."""
    if db is None:
        raise HTTPException(status_code=500, detail="Cơ sở dữ liệu chưa được khởi tạo")
    # Use dict excluding None so missing fields are not stored as null
    doc = data.dict(exclude_none=True)
    if doc.get("created_at") is None:
        doc["created_at"] = datetime.utcnow()

    sensor_keys = {"soil_moisture", "light", "temperature", "humidity", "water_level"}
    has_sensor_values = any(key in doc for key in sensor_keys)

    # Pump control policy (production-ready):
    # - None = no reading; do not treat as 0.
    # - Use hysteresis: turn ON when moisture < ON_THRESHOLD, turn OFF when > OFF_THRESHOLD.
    # - Debounce changes: at least DEBOUNCE seconds between actual pump state changes.
    ON_THRESHOLD = 30.0
    OFF_THRESHOLD = 40.0
    DEBOUNCE = 30  # seconds

    device = data.device_id or "ESP8266"

    # helper: get current pump state doc or default
    async def _get_pump_state(device_id: str):
        state = await db.pump_state.find_one({"device_id": device_id})
        if not state:
            # initialize
            now = datetime.utcnow()
            state = {"device_id": device_id, "pump_on": False, "last_changed": now}
            await db.pump_state.insert_one(state)
        return state

    async def _set_pump_state(device_id: str, pump_on: bool):
        now = datetime.utcnow()
        await db.pump_state.update_one({"device_id": device_id}, {"$set": {"pump_on": pump_on, "last_changed": now}}, upsert=True)

    state = await _get_pump_state(device)
    pump_on = bool(state.get("pump_on", False))
    last_changed = state.get("last_changed") or datetime.utcnow() - timedelta(seconds=DEBOUNCE + 1)
    if isinstance(last_changed, str):
        # in case it was serialized as string
        try:
            last_changed = datetime.fromisoformat(last_changed)
        except Exception:
            last_changed = datetime.utcnow() - timedelta(seconds=DEBOUNCE + 1)

    now = datetime.utcnow()
    elapsed = (now - last_changed).total_seconds()

    response = {"pump": pump_on, "reason": "no_change", "elapsed_since_change": int(elapsed)}
    # Thêm trường tiếng Việt để dễ đọc (không xoá các khóa tiếng Anh)
    response["trang_thai_bom"] = pump_on
    response["ly_do"] = "khong_thay_doi"
    response["thoi_gian_tu_thay_doi"] = int(elapsed)

    if not has_sensor_values:
        # No sensor values in payload — to be safe, turn the pump OFF if it was ON.
        if pump_on:
            await _set_pump_state(device, False)
            cmd = {
                "device_id": device,
                "command": "PUMP_OFF",
                "status": "executed",
                "created_at": datetime.utcnow(),
            }
            await db.watering_history.insert_one(cmd)
            response.update({"pump": False, "reason": "no_data_forced_off"})
            response["ly_do"] = "khong_co_du_lieu_tat_bom"
            response["trang_thai_bom"] = False
        else:
            response["reason"] = "no_data"
            response["ly_do"] = "khong_co_du_lieu"
            response["trang_thai_bom"] = pump_on
        return response

    await db.sensor_data.insert_one(doc)

    # Only make a decision if the payload actually included a soil_moisture field
    has_sm = "soil_moisture" in doc
    if not has_sm:
        response["reason"] = "no_data"
        response["ly_do"] = "khong_co_du_lieu"
        response["trang_thai_bom"] = pump_on
        return response

    sm = float(doc.get("soil_moisture"))

    # Decide
    # Safety override: if pump is ON and moisture is above OFF_THRESHOLD, turn it OFF immediately
    if pump_on and sm > OFF_THRESHOLD:
        await _set_pump_state(device, False)
        cmd = {
            "device_id": device,
            "command": "PUMP_OFF",
            "status": "executed",
            "created_at": datetime.utcnow(),
        }
        await db.watering_history.insert_one(cmd)
        response.update({"pump": False, "reason": "moisture_above_off_threshold_immediate"})
        response["ly_do"] = "do_am_tren_muc_tat_ngay"
        response["trang_thai_bom"] = False
    elif not pump_on and sm < ON_THRESHOLD:
        # turn on immediately when moisture below ON_THRESHOLD
        await _set_pump_state(device, True)
        duration = 10
        cmd = {
            "device_id": device,
            "command": "PUMP_ON",
            "duration": duration,
            "status": "executed",
            "created_at": datetime.utcnow(),
        }
        await db.watering_history.insert_one(cmd)
        response.update({"pump": True, "duration": duration, "reason": "moisture_below_on_threshold"})
        response["ly_do"] = "do_am_duoi_muc_bat"
        response["trang_thai_bom"] = True
        response["thoi_luong"] = duration
    else:
        # No state change: either within hysteresis band or debounced
        if elapsed < DEBOUNCE:
            response["reason"] = "debounced"
            response["ly_do"] = "da_duoc_giam_bop"
        else:
            response["reason"] = "within_hysteresis"
            response["ly_do"] = "trong_vung_hysteresis"

    return response


@app.get("/api/latest")
async def get_latest(device_id: Optional[str] = None):
    """Return latest sensor data for a device."""
    if db is None:
        raise HTTPException(status_code=500, detail="Database not initialized")

    query = {"device_id": device_id} if device_id else {}
    doc = await db.sensor_data.find(query).sort("created_at", -1).limit(1).to_list(length=1)
    if not doc:
        return {}
    return serialize_doc(doc[0])


@app.get("/api/history")
async def get_history(device_id: Optional[str] = None, limit: int = 100):
    """Return historical sensor data."""
    if db is None:
        raise HTTPException(status_code=500, detail="Database not initialized")

    query = {"device_id": device_id} if device_id else {}
    docs = await db.sensor_data.find(query).sort("created_at", -1).to_list(length=limit)
    return [serialize_doc(d) for d in docs]


@app.get("/api/watering-history")
async def get_watering_history(device_id: Optional[str] = None, limit: int = 100):
    """Return watering history."""
    if db is None:
        raise HTTPException(status_code=500, detail="Database not initialized")

    query = {"device_id": device_id} if device_id else {}
    docs = await db.watering_history.find(query).sort("created_at", -1).to_list(length=limit)
    return [serialize_doc(d) for d in docs]


def serialize_doc(doc):
    """Convert MongoDB document (ObjectId, datetime) into JSON-serializable dict."""
    if doc is None:
        return doc
    out = {}
    for k, v in doc.items():
        if isinstance(v, ObjectId):
            out[k] = str(v)
        elif isinstance(v, datetime):
            # Serialize datetime with explicit timezone offset so browser parses local VN time correctly.
            try:
                if v.tzinfo is None:
                    v = v.replace(tzinfo=timezone.utc)
                if ZoneInfo is not None:
                    local = v.astimezone(ZoneInfo("Asia/Ho_Chi_Minh"))
                else:
                    local = v.astimezone(timezone(timedelta(hours=7)))
            except Exception:
                local = v
            out[k] = local.isoformat()
        elif isinstance(v, list):
            new_list = []
            for item in v:
                if isinstance(item, dict):
                    new_list.append(serialize_doc(item))
                else:
                    new_list.append(item)
            out[k] = new_list
        elif isinstance(v, dict):
            out[k] = serialize_doc(v)
        else:
            out[k] = v
    return out


@app.post("/api/pump")
async def post_pump(cmd: PumpCommand):
    """Manual pump control command: store and return accepted."""
    if db is None:
        raise HTTPException(status_code=500, detail="Cơ sở dữ liệu chưa được khởi tạo")

    doc = cmd.dict()
    now = datetime.utcnow()
    doc["status"] = "executed"
    doc["created_at"] = now
    await db.watering_history.insert_one(doc)

    # Update pump_state collection so UI can reflect current pump status
    pump_on = True if cmd.command == "PUMP_ON" else False
    await db.pump_state.update_one({"device_id": cmd.device_id}, {"$set": {"pump_on": pump_on, "last_changed": now}}, upsert=True)

    return {"status": "accepted", "pump": pump_on, "trang_thai_bom": pump_on}


@app.get("/api/pump/state")
async def get_pump_state(device_id: Optional[str] = None):
    """Return pump state for a device (pump_on, last_changed)."""
    if db is None:
        raise HTTPException(status_code=500, detail="Cơ sở dữ liệu chưa được khởi tạo")
    device = device_id or "ESP8266"
    state = await db.pump_state.find_one({"device_id": device})
    if not state:
        # default state
        now = datetime.utcnow()
        state = {"device_id": device, "pump_on": False, "last_changed": now}
        await db.pump_state.insert_one(state)
    return serialize_doc(state)

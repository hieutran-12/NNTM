# Hướng Dẫn Cài Đặt Firmware: Arduino + ESP8266

## Giới Thiệu

- **Arduino**: Đọc cảm biến độ ẩm đất và gửi dữ liệu JSON qua UART tới ESP8266.
- **ESP8266**: Ưu tiên nhận dữ liệu từ Arduino qua UART, nếu không có dữ liệu thì đọc từ Serial Monitor. Sau đó gửi tới backend API và điều khiển relay bơm.

## Sơ Đồ Nối Dây

### Kết Nối Cảm Biến với Arduino

```
Cảm biến độ ẩm đất (analog) → A0
GND                          → GND
+5V                          → +5V
```

### Kết Nối Arduino ↔ ESP8266

```
Arduino TX (pin 1) → ESP8266 RX (D5)
Arduino GND        → ESP8266 GND
```

> Lưu ý: Chỉ cần nối Arduino TX tới ESP8266 RX nếu ESP8266 chỉ nhận dữ liệu từ Arduino. Nếu bạn cần trả dữ liệu về Arduino thì mới cần nối thêm ESP8266 TX tới Arduino RX.

### Kết Nối Relay bơm với ESP8266

```
Relay module signal → D1 (GPIO5)
Relay module GND    → GND
Relay module VCC    → 5V hoặc 3.3V (theo module)
```

### Cấp nguồn cho ESP8266

- ESP8266 cần nguồn ổn định 3.3V/5V tuỳ board.
- Chung đất (GND) giữa ESP8266, Arduino và module relay.

## Hướng Dẫn Cài Đặt Arduino

1. **Cài đặt thư viện ArduinoJSON**:
   - Arduino IDE → Sketch → Include Library → Manage Libraries
   - Tìm "ArduinoJSON" và cài đặt phiên bản mới nhất

2. **Upload file `arduino_sensor_reader.ino`**:
   - Mở Arduino IDE
   - Chọn Board: Arduino Uno
   - Chọn COM port
   - Nhấn Upload

3. **Kiểm tra**:
   - Mở Serial Monitor (115200 baud)
   - Nếu thành công, sẽ in ra "Arduino ready!"

## Hướng Dẫn Cài Đặt ESP8266

1. **Cài đặt hỗ trợ Board ESP8266**:
   - Arduino IDE → File → Preferences
   - Additional Boards Manager URLs: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board → Boards Manager → Tìm "ESP8266" → Cài đặt

2. **Cấu hình WiFi & API** (chỉnh sửa trong file `esp8266_controller.ino`):

   ```cpp
   const char* SSID = "TEN_WIFI_CUA_BAN";
   const char* PASSWORD = "MAT_KHAU_WIFI";
   const char* API_URL = "http://192.168.x.x:8000/api/sensor";
   ```

3. **Upload file `esp8266_controller.ino`**:
   - Chọn Board: NodeMCU 1.0 (hoặc board ESP8266 phù hợp)
   - Chọn COM port
   - Nhấn Upload

4. **Kiểm tra**:
   - Mở Serial Monitor (115200 baud)
   - ESP8266 sẽ in log kết nối WiFi, dữ liệu Arduino, và payload gửi lên API

## Nguyên Tắc Ưu Tiên Dữ Liệu

1. ESP8266 kiểm tra dữ liệu từ Arduino qua UART.
2. Nếu có dữ liệu Arduino hợp lệ, ESP8266 dùng dữ liệu đó.
3. Nếu không có dữ liệu Arduino, ESP8266 kiểm tra Serial Monitor.
4. Nếu vẫn không có, ESP8266 dùng giá trị mặc định `0` cho các trường thiếu.
5. Dữ liệu gửi tới API mỗi 30 giây.

## Định Dạng Dữ Liệu Chuẩn

ESP8266 sẽ gửi JSON chuẩn tới backend:

```json
{
  "device_id": "ESP8266",
  "soil_moisture": 45,
  "light": 700,
  "temperature": 30,
  "humidity": 65,
  "water_level": 80
}
```

Nếu trường nào thiếu hoặc null thì sẽ tự bổ sung `0` trước khi gửi.

## Thử Nghiệm

1. **Khởi động backend**:

   ```bash
   uvicorn app.main:app --reload --port 8000
   ```

2. **Upload Arduino và ESP8266** lên board tương ứng.

3. **Kết nối Serial Monitor** tới ESP8266 (115200 baud) để xem log.

4. **Gửi dữ liệu thử qua Serial Monitor** (nếu chưa có Arduino):

   ```json
   { "soil_moisture": 40, "light": 500 }
   ```

## Khắc Phục Sự Cố

- **Không nhận dữ liệu Arduino**: kiểm tra dây nối TX-Arduino → RX-ESP8266 và chung GND.
- **ESP8266 không kết nối WiFi**: kiểm tra SSID/mật khẩu và nguồn cấp.
- **Không gửi được API**: kiểm tra `API_URL` và backend đang chạy.
- **Relay bơm không hoạt động**: kiểm tra chân D1 và nguồn relay.

## Lưu ý

- Nếu Arduino chỉ gửi một số trường, ESP8266 sẽ tự thêm trường còn thiếu với giá trị `0`.
- Dữ liệu từ Serial Monitor chỉ dùng khi không có dữ liệu mới từ Arduino.

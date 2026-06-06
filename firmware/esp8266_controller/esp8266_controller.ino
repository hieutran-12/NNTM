// ESP8266 WiFi gateway for backend connection
// Receives sensor data from Arduino via UART
// Sends normalized JSON to backend every 30 seconds
// Receives pump command from backend and forwards it back to Arduino

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

WiFiClient wifiClient;

// WiFi credentials
const char* SSID = "WIFI GIANG VIEN";
const char* PASSWORD = "GV#dainam@5577";

// Backend API
const char* API_URL = "http://192.168.61.237:8000/api/sensor";
const char* DEVICE_ID = "ESP8266";

// Hardware pins
const int ARDUINO_RX_PIN = 14; // D5 / connect to Arduino TX
const int ARDUINO_TX_PIN = 12; // D6 / connect to Arduino RX (optional)

SoftwareSerial arduinoSerial(ARDUINO_RX_PIN, ARDUINO_TX_PIN);

const unsigned long SEND_INTERVAL = 30000;
const unsigned long PUMP_CHECK_INTERVAL = 5000;
unsigned long lastSendTime = 0;
unsigned long lastPumpCheckTime = 0;
bool lastKnownPumpOn = false;
bool lastKnownPumpAvailable = false;

String readLine(Stream &stream, unsigned long timeoutMs) {
  static String buffer = "";
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (stream.available()) {
      char c = stream.read();
      if (c == '\r') continue;
      if (c == '\n') {
        String line = buffer;
        buffer = "";
        return line;
      }
      buffer += c;
      if (buffer.length() > 1024) {
        buffer = buffer.substring(buffer.length() - 1024);
      }
    }
  }

  return "";
}

bool parseJsonInput(const String &input, StaticJsonDocument<256> &doc) {
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    return false;
  }
  return true;
}

bool parseCSVInput(const String &input, StaticJsonDocument<256> &doc) {
  // Format: soil_moisture,light,temperature,humidity,water_level
  // Example: 25,700,26,55,2
  int count = 0;
  int start = 0;
  float values[5] = {0, 0, 0, 0, 0};
  
  for (int i = 0; i <= input.length(); i++) {
    if (i == input.length() || input[i] == ',') {
      String part = input.substring(start, i);
      part.trim();
      if (count < 5 && part.length() > 0) {
        values[count] = part.toFloat();
      }
      count++;
      start = i + 1;
    }
  }
  
  if (count >= 5) {
    doc["soil_moisture"] = values[0];
    doc["light"] = values[1];
    doc["temperature"] = values[2];
    doc["humidity"] = values[3];
    doc["water_level"] = values[4];
    return true;
  }
  return false;
}

float getValue(JsonVariant var) {
  if (!var || var.isNull()) {
    return 0.0;
  }
  return var.as<float>();
}

String buildPayload(JsonVariant source) {
  StaticJsonDocument<256> payload;
  payload["device_id"] = DEVICE_ID;

  // Chỉ thêm các trường nếu nguồn có khóa tương ứng để tránh gửi giá trị mặc định 0
  if (source.containsKey("soil_moisture")) {
    payload["soil_moisture"] = getValue(source["soil_moisture"]);
  } else if (source.containsKey("moisture")) {
    payload["soil_moisture"] = getValue(source["moisture"]);
  }

  if (source.containsKey("light")) {
    payload["light"] = getValue(source["light"]);
  }

  if (source.containsKey("temperature")) {
    payload["temperature"] = getValue(source["temperature"]);
  } else if (source.containsKey("temp")) {
    payload["temperature"] = getValue(source["temp"]);
  }

  if (source.containsKey("humidity")) {
    payload["humidity"] = getValue(source["humidity"]);
  } else if (source.containsKey("hum")) {
    payload["humidity"] = getValue(source["hum"]);
  }

  if (source.containsKey("water_level")) {
    payload["water_level"] = getValue(source["water_level"]);
  } else if (source.containsKey("waterLevel")) {
    payload["water_level"] = getValue(source["waterLevel"]);
  }

  String output;
  serializeJson(payload, output);
  return output;
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed to connect");
  }
}

void forwardPumpCommand(bool pumpOn) {
  String command = pumpOn ? "PUMP_ON" : "PUMP_OFF";
  Serial.println("Gửi lệnh tới Arduino: " + command);
  arduinoSerial.println(command);
}

void pollPumpCommand() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
  }

  String pumpStateUrl = String(API_URL);
  pumpStateUrl.replace("/api/sensor", "/api/pump/state");

  HTTPClient http;
  http.begin(wifiClient, pumpStateUrl);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error && responseDoc.containsKey("pump_on")) {
      bool pumpOn = responseDoc["pump_on"].as<bool>();
      if (!lastKnownPumpAvailable || pumpOn != lastKnownPumpOn) {
        lastKnownPumpOn = pumpOn;
        lastKnownPumpAvailable = true;
        forwardPumpCommand(pumpOn);
      }
    }
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\nESP8266 gateway khởi động...");
  Serial.println("Định dạng đầu vào Serial (từ Arduino):");
  Serial.println("  JSON: {\"soil_moisture\":25,\"light\":700,...}");
  Serial.println("  CSV:  25,700,26,55,2");
  Serial.println("  (Thứ tự CSV: moisture, light, temp, humidity, water_level)");

  // Use 4800 for SoftwareSerial to improve reliability on ESP8266
  arduinoSerial.begin(4800);
  Serial.println("SoftwareSerial ESP8266 <-> Arduino: 4800 baud");

  connectWiFi();
}

void loop() {
  if (millis() - lastPumpCheckTime >= PUMP_CHECK_INTERVAL) {
    lastPumpCheckTime = millis();
    pollPumpCommand();
  }

  if (millis() - lastSendTime < SEND_INTERVAL) {
    delay(50);
    return;
  }
  lastSendTime = millis();

  StaticJsonDocument<256> dataDoc;
  String source = "default";

  // Give the Arduino up to 5s to send a full line (SoftwareSerial can be slow)
  String arduinoLine = readLine(arduinoSerial, 5000);
  if (arduinoLine.length() > 0) {
    Serial.println("Dữ liệu từ Arduino (UART): " + arduinoLine);
    if (parseJsonInput(arduinoLine, dataDoc)) {
      source = "Arduino";
    }
  }

  if (source == "default") {
    String monitorLine = "";
    // Quick read if user already typed something
    if (Serial.available()) {
      monitorLine = readLine(Serial, 200);
    }

    // If no input from Serial immediately, give the user a chance to type
    if (monitorLine.length() == 0) {
      Serial.println("No Arduino data. You can type JSON or CSV in Serial Monitor within 5 seconds...");
      monitorLine = readLine(Serial, 5000);
    }

    if (monitorLine.length() > 0) {
      Serial.println("Dữ liệu từ Serial Monitor: " + monitorLine);
      // Try JSON first
      if (parseJsonInput(monitorLine, dataDoc)) {
        source = "Serial JSON";
      } 
      // If JSON fails, try CSV
      else if (parseCSVInput(monitorLine, dataDoc)) {
        source = "Serial CSV";
        Serial.println("Đã phân tích CSV: do_am=" + String(dataDoc["soil_moisture"].as<float>()) + 
                       ", sang=" + String(dataDoc["light"].as<float>()) + 
                       ", nhiet_do=" + String(dataDoc["temperature"].as<float>()) +
                       ", do_am_khong_khi=" + String(dataDoc["humidity"].as<float>()) +
                       ", muc_nuoc=" + String(dataDoc["water_level"].as<float>()));
      }
      else {
        Serial.println("Định dạng không hợp lệ. Dùng JSON hoặc CSV: moisture,light,temp,humidity,water");
      }
    }
  }

  if (source == "default") {
    Serial.println("Không có dữ liệu từ Arduino hoặc Serial Monitor. Sử dụng giá trị mặc định.");
    dataDoc.clear();
  }

  String payload = buildPayload(dataDoc.as<JsonVariant>());
  Serial.println("Dữ liệu gửi: " + payload);

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(wifiClient, API_URL);
    http.addHeader("Content-Type", "application/json");

    Serial.println("Gửi tới API: " + String(API_URL));
    Serial.println("Trạng thái WiFi: " + String(WiFi.status()));

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Phản hồi API (" + String(httpCode) + "): " + response);

      // Parse response and send pump command to Arduino
      if (httpCode == 200) {
        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, response);
        if (!error && responseDoc.containsKey("pump")) {
          bool pumpStatus = responseDoc["pump"].as<bool>();
          String command = pumpStatus ? "PUMP_ON" : "PUMP_OFF";
          
          Serial.println("Gửi lệnh tới Arduino: " + command);
          arduinoSerial.println(command);
          delay(100); // Wait for Arduino to process
        } else {
          Serial.println("Lỗi giải nén response hoặc không có trường 'pump'");
        }
      }
    } else {
      Serial.println("Lỗi HTTP: " + String(httpCode));
      Serial.println("Kiểm tra backend có thể truy cập được từ thiết bị hoặc URL API có chính xác không.");
    }
    http.end();
  } else {
    Serial.println("Cannot send payload: WiFi disconnected");
  }
}

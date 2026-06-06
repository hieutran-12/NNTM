// Arduino Sensor Reader
// Reads a moisture sensor and sends JSON data via SoftwareSerial to ESP8266.

#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Sensor pins
const int SOIL_MOISTURE_PIN = A0;
const int DHT_PIN = 2; // DHT22 data pin
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

const int RELAY_PIN = 3; // Arduino pin connected to relay IN
const bool RELAY_ACTIVE_LOW = false; // đổi sang false nếu relay là active-high

// Uno pins for SoftwareSerial to ESP8266
const int ESP_RX_PIN = 10; // Arduino receive pin, connect to ESP TX
const int ESP_TX_PIN = 11; // Arduino transmit pin, connect to ESP RX

SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 10000; // 10 seconds for faster testing

void setup() {
  // USB debug serial
  Serial.begin(9600);
  delay(100);
  Serial.println("Arduino UNO sẵn sàng.");
  Serial.println("Debug trên USB 9600.");

  // Relay control pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  Serial.println("Relay pin đã được cấu hình.");

  // DHT22 initialization
  dht.begin();
  Serial.println("DHT22 đã được khởi tạo trên chân D2.");

  // Serial to ESP8266
  espSerial.begin(4800);
  Serial.println("Kết nối ESP8266 qua SoftwareSerial 4800.");
  Serial.println("Nối: UNO D11 -> ESP RX, UNO D10 <- ESP TX, chung GND.");
}

void sendSensorData() {
  int soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
  float soilMoisture = map(soilMoistureRaw, 0, 1023, 0, 100);

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Cảnh báo: không đọc được DHT22. Gán giá trị 0.");
    temperature = 0;
    humidity = 0;
  }

  StaticJsonDocument<256> doc;
  doc["device_id"] = "ESP8266";
  doc["soil_moisture"] = round(soilMoisture * 10) / 10.0;
  doc["temperature"] = round(temperature * 10) / 10.0;
  doc["humidity"] = round(humidity * 10) / 10.0;
  doc["light"] = 0;
  doc["water_level"] = 0;

  String payload;
  serializeJson(doc, payload);

  Serial.println("Gửi payload tới ESP8266:");
  Serial.println(payload);
  espSerial.println(payload);
}

void checkPumpCommand() {
  if (espSerial.available()) {
    String line = espSerial.readStringUntil('\n');
    line.trim();
        if (line.length() > 0) {
        Serial.println("Nhận lệnh từ ESP8266: " + line);
        if (line == "PUMP_ON") {
          Serial.println("Lệnh bơm: BẬT");
          digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
          Serial.print("Relay state: ");
          Serial.println(RELAY_ACTIVE_LOW ? "LOW (ON)" : "HIGH (ON)");
        } else if (line == "PUMP_OFF") {
          Serial.println("Lệnh bơm: TẮT");
          digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
          Serial.print("Relay state: ");
          Serial.println(RELAY_ACTIVE_LOW ? "HIGH (OFF)" : "LOW (OFF)");
        } else {
          Serial.println("Lệnh không xác định.");
        }
      }
  }
}

void loop() {
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();
    sendSensorData();
  }

  checkPumpCommand();
}

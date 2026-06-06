# Hướng dẫn lắp dây Relay với máy bơm và cảm biến

## 1. Tổng quan luồng tín hiệu

- Arduino đọc cảm biến đất, pH, độ ẩm, điện trở dẫn nước.
- Arduino gửi dữ liệu sang ESP8266 qua UART (SoftwareSerial).
- ESP8266 chỉ làm gateway: nhận dữ liệu từ Arduino, gửi payload lên backend.
- Backend quyết định bật/tắt bơm và trả về lệnh `pump: true/false`.
- ESP8266 nhận response và gửi lệnh `PUMP_ON` / `PUMP_OFF` trở lại Arduino qua UART.
- Arduino nhận lệnh và điều khiển module relay trực tiếp để bật/tắt máy bơm.

## 2. Phần cứng cần chuẩn bị

- Arduino Uno / Nano / Pro Mini
- ESP8266 (ví dụ NodeMCU, Wemos D1 mini)
- Module relay 1 kênh (5V)
- Máy bơm phù hợp với relay (DC hoặc AC tùy loại)
- Cảm biến độ ẩm đất / pH / độ dẫn / nhiệt độ
- Dây nối và nguồn cấp điện riêng cho máy bơm, Arduino, ESP8266
- Cần chung mass (GND) giữa Arduino và ESP8266

## 3. Kết nối cơ bản

### 3.1 Kết nối cảm biến với Arduino

(Các chân ví dụ; tùy từng cảm biến, đổi theo phần cứng thực tế)

- Cảm biến độ ẩm đất:
  - VCC → 5V
  - GND → GND
  - OUT / analog → A0

- Cảm biến DHT22:
  - VCC → 5V
  - GND → GND
  - DATA → Arduino D2 (có pull-up nội)
  - Nếu module DHT22 không có điện trở kéo lên, dùng thêm điện trở 4.7k-10k giữa DATA và VCC

- Cảm biến pH / EC / nhiệt độ khác: theo sơ đồ cảm biến của bạn
  - VCC → 5V hoặc 3.3V
  - GND → GND
  - Data → các chân analog/digital tương ứng

### 3.2 Kết nối Arduino và ESP8266 qua UART

- Arduino TX → ESP8266 RX (chân `ARDUINO_RX_PIN` = GPIO14 / D5) qua `SoftwareSerial`
- Arduino RX → ESP8266 TX (chân `ARDUINO_TX_PIN` = GPIO12 / D6) nếu dùng 2 chiều
- Arduino GND → ESP8266 GND

> Lưu ý: ESP8266 chạy 3.3V trong khi Arduino thường chạy 5V. Nếu Arduino dùng UART 5V, nên thêm divider hoặc module level shifter cho chân RX của ESP8266 để tránh quá áp.

### 3.3 Kết nối ESP8266 nguồn và WiFi

- ESP8266 cấp điện bằng nguồn 5V vào VIN hoặc cổng USB.
- Đảm bảo nguồn ổn định tối thiểu 500mA - 1A cho ESP8266 khi dùng WiFi.

## 4. Kết nối Relay với Arduino và máy bơm

### 4.1 Relay module 5V cơ bản

- VCC → 5V Arduino
- GND → GND chung với Arduino
- IN → chân digital Arduino điều khiển relay

### 4.2 Kết nối máy bơm qua relay

Tùy máy bơm AC hoặc DC, dùng NO (Normally Open) để đảm bảo máy bơm tắt khi relay không cấp điện.

#### Nếu máy bơm DC:

- Relay COM → nguồn dương + của máy bơm
- Relay NO → nguồn dương + của nguồn máy bơm
- Máy bơm âm (-) → nguồn âm của nguồn máy bơm

#### Nếu máy bơm AC:

- Relay COM → dây nóng (Live) từ nguồn AC
- Relay NO → dây nóng vào máy bơm
- Dây trung tính (Neutral) trực tiếp đến máy bơm

### 4.3 Ví dụ chân điều khiển relay trên Arduino

Giả sử ta dùng chân `D3` trên Arduino:

- Arduino D3 → Relay IN

Arduino sẽ bật relay khi nhận lệnh `PUMP_ON` từ ESP8266 và tắt relay khi nhận `PUMP_OFF`.

Nếu module relay của bạn là active-low (phổ biến với relay 5V):

- `HIGH` = tắt relay
- `LOW` = bật relay

Nếu module relay là active-high thì ngược lại:

- `HIGH` = bật relay
- `LOW` = tắt relay

Trong sketch Arduino hiện tại, mình đã đặt `RELAY_ACTIVE_LOW = false`, tức là dùng kiểu active-high. Nếu relay của bạn vẫn bật khi gửi `PUMP_OFF`, hãy kiểm tra lại:

- dây `Relay IN` có đúng vào chân `D3` không
- dây nguồn bơm có đi qua `COM` và `NO` trên relay không
- đèn LED relay khi `PUMP_OFF` có tắt hay không

## 5. Sơ đồ đấu dây đề xuất

```
Arduino            ESP8266            Relay module          Máy bơm
-------            --------            ------------          -------
  TX  ---------->  RX (GPIO14)
  RX  <----------  TX (GPIO12)
  GND ---------->  GND
  5V  ---------->  5V

  D3   ---------->  IN
  5V   ---------->  VCC
  GND  ---------->  GND

Relay COM -------> + nguồn máy bơm
Relay NO --------> + máy bơm
Máy bơm - -------> - nguồn máy bơm
```

## 6. Lưu ý an toàn

- Tuyệt đối dùng nguồn riêng cho máy bơm và relay, tránh cấp nguồn cho máy bơm từ ESP/Arduino.
- Relay chỉ là cầu đóng ngắt nguồn, không phải nguồn cấp. Cần chọn relay phù hợp với dòng/điện áp máy bơm.
- Nếu dùng máy bơm AC, nên có cầu chì và bảo vệ chống rò điện.
- Kiểm tra kỹ dây GND chung giữa Arduino và ESP8266 để tín hiệu UART truyền ổn định.
- Không nối thẳng 5V Arduino vào RX của ESP8266 nếu chưa dùng chuyển mức logic.

## 7. Kiểm tra sau khi đấu dây

1. Cấp nguồn cho Arduino và ESP8266.
2. Mở Serial Monitor của Arduino/ESP8266 để quan sát log.
3. Gửi dữ liệu thử qua Arduino hoặc bật nguồn cảm biến.
4. Quan sát ESP8266 vừa POST tới backend vừa gửi lệnh `PUMP_ON` / `PUMP_OFF` về Arduino.
5. Arduino nhận lệnh và đóng/mở relay.
6. Máy bơm bật/tắt theo lệnh nếu mọi dây đã đúng.

## 8. Gợi ý thêm

- Nếu cần, dùng ngõ `Serial` USB Arduino để debug lệnh `PUMP_ON`/`PUMP_OFF` trước khi đấu vào ESP.
- Có thể dùng `LED` tạm trên relay IN để kiểm tra tín hiệu điều khiển.
- Nếu không rõ loại máy bơm, chụp ảnh và kiểm tra thông số điện áp/dòng để chọn relay đúng.

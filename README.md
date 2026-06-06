# 🌱 Soil Monitoring & Pump Control System

<p align="center">
  <img src="image.png" width="800"/>
</p>

<p align="center">
  🚀 Hệ thống IoT giám sát đất & điều khiển bơm tự động theo thời gian thực
</p>

---

## 🧠 Giới thiệu

Đây là hệ thống IoT giúp **giám sát môi trường đất và điều khiển bơm nước từ xa**, sử dụng:

- ESP8266 + Arduino để thu thập dữ liệu cảm biến
- FastAPI làm backend xử lý dữ liệu
- MongoDB lưu trữ dữ liệu
- React hiển thị dashboard trực quan

---

## ⚙️ Tính năng chính

✅ Theo dõi dữ liệu cảm biến realtime  
✅ Biểu đồ lịch sử (Chart.js)  
✅ Điều khiển bơm từ giao diện web  
✅ Lưu lịch sử tưới & trạng thái bơm  
✅ Hỗ trợ nhiều thiết bị (`device_id`)  

---

## 🧩 Tech Stack

<p>
  <img src="https://skillicons.dev/icons?i=python,fastapi,react,vite,mongodb,arduino,js" />
</p>

---

## 🏗️ Kiến trúc hệ thống

```text
[ Sensors ]
     ↓
[ Arduino ]
     ↓ UART
[ ESP8266 ]  →  HTTP →  [ FastAPI Backend ] → [ MongoDB ]
                                      ↓
                                [ React Dashboard ]

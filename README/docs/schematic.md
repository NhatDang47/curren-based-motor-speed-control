# KẾN TRÚC PHẦN CỨNG: HỆ THỐNG ĐIỀU KHIỂN PID DÒNG ĐIỆN (ESP32)

Tài liệu này mô tả sơ đồ đấu nối vật lý, phân bổ tài nguyên GPIO và quy hoạch nguồn điện cho hệ thống điều khiển động cơ DC sử dụng vi điều khiển ESP32.

## 1. Danh sách linh kiện
* **Vi điều khiển:** ESP32 DoIT DevKit V1 (hoặc tương đương).
* **Driver Động cơ:** L298N (Mạch cầu H - BJT).
* **Cảm biến dòng điện:** INA219 (Giao tiếp I2C).
* **Hiển thị:** Màn hình OLED SSD1306 0.96 inch (Giao tiếp I2C).
* **Cảm biến tốc độ:** Encoder quang/từ gắn tích hợp trên trục động cơ.
* **Giao tiếp HMI:** 1x Biến trở (10kΩ), 4x Nút nhấn nhả.

## 2. Quy hoạch Nguồn điện
Hệ thống sử dụng chiến lược chia tải để triệt tiêu hiện tượng sụt áp (Brownout Reset) cho vi điều khiển khi động cơ khởi động hoặc bật Wi-Fi:

| Nguồn cấp | Thiết bị tiêu thụ | Ghi chú an toàn |
| :--- | :--- | :--- |
| **12V DC** | Domino `12V` của L298N | Nguồn công suất chính cho động cơ. |
| **5V DC (VIN)** | Cấp nguồn ESP32, OLED VCC | OLED dùng nguồn 5V ngoài để giảm tải cho IC LDO 3.3V của ESP32. |
| **3.3V DC** | INA219 VCC, Nút nhấn, Biến trở | Nguồn tín hiệu nội bộ từ pin `3.3V` của ESP32. |
| **GND** | Nối chung TOÀN BỘ hệ thống | Bắt buộc nối chung GND giữa mạch công suất (L298N) và mạch tín hiệu (ESP32). |

## 3. Bảng Phân bổ Chân tín hiệu

### 3.1. Khối Công suất & Truyền động (L298N & Motor)
| Chân ESP32 | Linh kiện | Chức năng (Function) | Mức Logic / Config |
| :--- | :--- | :--- | :--- |
| `GPIO_NUM_13` | L298N - `ENA` | Điều chế độ rộng xung (PWM) | Tần số 5000Hz, 8-bit (0-255) |
| `GPIO_NUM_14` | L298N - `IN1` | Chiều quay thuận (Forward) | Digital Output |
| `GPIO_NUM_27` | L298N - `IN2` | Chiều quay nghịch (Reverse) | Digital Output |

### 3.2. Khối Phản hồi & Cảm biến
| Chân ESP32 | Linh kiện | Chức năng (Function) | Mức Logic / Config |
| :--- | :--- | :--- | :--- |
| `GPIO_NUM_4`  | Encoder - `Phase A/B` | Đếm xung tốc độ (RPM) | External Interrupt, Pull-up |
| `GPIO_NUM_34` | Biến trở (Center Pin) | Đọc giá trị Setpoint / Kp / Ki | ADC1_CHANNEL_6, Atten 11dB (0-3.3V) |

### 3.3. Khối Giao tiếp I2C (I2C Buses)
Sử dụng cả 2 lõi I2C phần cứng của ESP32 để tránh thắt cổ chai tín hiệu giữa việc đọc Cảm biến (ưu tiên cao) và Vẽ màn hình (ưu tiên thấp).

| Bus phần cứng | Chân ESP32 | Thiết bị | Địa chỉ I2C | Tốc độ |
| :--- | :--- | :--- | :--- | :--- |
| **I2C_NUM_0** | `SDA = 21`, `SCL = 19` | INA219 (Đo dòng điện) | `0x40` | 400 kHz (Fast Mode) |
| **I2C_NUM_1** | `SDA = 22`, `SCL = 23` | OLED SSD1306 | `0x3C` | 400 kHz (Fast Mode) |

### 3.4. Khối Giao diện Người dùng (HMI Buttons)
Các nút nhấn được mắc theo cấu hình **Kéo lên tích cực mức THẤP (Active LOW)**. Nối 1 đầu nút nhấn vào GPIO, đầu còn lại nối trực tiếp GND. Sử dụng điện trở Pull-up nội bộ của ESP32.

| Chân ESP32 | Tên Nút | Chức năng phần mềm |
| :--- | :--- | :--- |
| `GPIO_NUM_26` | `BTN_UP` | Chạy / Dừng Động cơ (Start/Stop) |
| `GPIO_NUM_18` | `BTN_DOWN` | Chuyển đổi tham số (Select Kp/Ki/Dir) |
| `GPIO_NUM_5`  | `BTN_LEFT` | Lùi Mode hiển thị |
| `GPIO_NUM_25` | `BTN_RIGHT`| Tiến Mode hiển thị (Mode 1 -> 5) |

## 4. Lưu ý Thiết kế & Debug
* **Triệt nhiễu ADC:** Tín hiệu biến trở tại chân 34 được lấy mẫu 10 lần và lọc thông thấp (Low-pass filter: 80% cũ + 20% mới) bằng phần mềm để ổn định Setpoint.


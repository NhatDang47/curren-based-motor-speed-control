

### 2. MATERIALS AND METHODS (VẬT LIỆU VÀ PHƯƠNG PHÁP)

**2.1. Mô hình hóa Động cơ DC (System Modeling)**
Hệ thống điều khiển được xây dựng dựa trên mô hình không gian trạng thái (State-space) của động cơ DC chổi than. Mối liên kết điện - cơ được mô tả bằng hệ phương trình vi phân:
* Động lực học phần ứng điện:
  $$\frac{di_a}{dt} = -\frac{R_a}{L_a}i_a - \frac{K_e}{L_a}\omega_m + \frac{1}{L_a}V_a$$
* Động lực học cơ khí:
  $$\frac{d\omega_m}{dt} = \frac{K_t}{J}i_a - \frac{B}{J}\omega_m - \frac{1}{J}T_L$$

Trong đó: $V_a$ là điện áp cấp (tỷ lệ với xung PWM), $i_a$ là dòng điện phần ứng, $\omega_m$ là vận tốc góc, $e_b = K_e \cdot \omega_m$ là sức điện động ngược, $T_L$ là mô-men tải cản.
Mục tiêu cốt lõi: Thiết kế bộ điều khiển PI ghim đại lượng $i_a$ bám sát giá trị đặt (Setpoint). Do mô-men sinh ra $\tau_m = K_t \cdot i_a$, việc điều khiển dòng điện chính là điều khiển trực tiếp lực kéo của động cơ, bất chấp sự biến thiên của sức điện động ngược $e_b$ do tải thay đổi.

**2.2. Tham số hóa Mô hình và Giả lập (Parameterization & Simulation)**
Để kiểm chứng độ ổn định của thuật toán PI số trước khi triển khai phần cứng, quy trình Thiết kế Dựa trên Mô hình (Model-Based Design) được áp dụng. Việc đo đạc chính xác các thông số động lực học như mô-men quán tính ($J$) và hệ số ma sát nhớt ($B$) đòi hỏi thiết bị chuyên dụng (Dynamometer). Do đó, nghiên cứu sử dụng bộ thông số của một động cơ DC công nghiệp chuẩn làm Mô hình Tham chiếu (Reference Plant). 

Bộ tham số này được trích xuất trực tiếp từ nghiên cứu nhận dạng hệ thống bằng mạng nơ-ron của Al-Qassar et al. (2008) [cite_start]và được ứng dụng làm benchmark trong nghiên cứu điều khiển vòng kín của Elhamid (2012)[cite: 146, 147, 380, 381]:
* [cite_start]Điện trở phần ứng: $R_a = 2.3 \Omega$ [cite: 148]
* [cite_start]Cảm kháng: $L_a = 0.03 H$ [cite: 148]
* [cite_start]Mô-men quán tính: $J = 0.045 kg.m^2$ [cite: 149]
* [cite_start]Hệ số ma sát nhớt: $B = 0.007 Nm/(rad/s)$ [cite: 150]
* [cite_start]Hằng số mô-men và sức điện động ngược: $K_t = 0.62 Nm/A, K_e = 0.64 V/(rad/s)$ [cite: 150]

Mục tiêu của bước mô phỏng với Mô hình Tham chiếu không phải là trích xuất bộ hệ số $K_p, K_i$ tuyệt đối cho động cơ vật lý, mà nhằm giải quyết hai bài toán cốt lõi: 
1. Cô lập lỗi (Bug Isolation): Xác thực tính đúng đắn của phương trình PI rời rạc hóa ở chu kỳ lấy mẫu $\Delta t = 50ms$. 
2. [cite_start]Chứng minh năng lực của vòng lặp dòng điện (Current Loop) trong việc bám dính giá trị đặt $10A$ và kháng nhiễu tải cơ học $T_L = 12 Nm$[cite: 163, 164]. 

Kết quả mô phỏng đóng vai trò là điểm neo (Baseline) toán học. Quá trình chuyển giao từ mô phỏng sang thực tế (Sim-to-Real Transfer) được thực hiện thông qua bước tinh chỉnh vi phân (Fine-tuning) trực tiếp trên vi điều khiển ESP32. Cơ chế giám sát qua Web SCADA cho phép hiệu chỉnh $K_p, K_i$ thời gian thực để bù trừ các đặc tính phi tuyến chưa được mô hình hóa của phần cứng, cụ thể là độ sụt áp của IC công suất L298N và nhiễu lấy mẫu từ cảm biến INA219.

---

**2.3. Thiết lập Phần cứng Thực nghiệm (Hardware Setup)**
Hệ thống vật lý được thi công để kiểm chứng thuật toán:
* **Vi xử lý trung tâm:** ESP32 DoIT DevKit V1 (240MHz).
* **Khối Công suất:** Mạch cầu H L298N. Tần số điều chế độ rộng xung (PWM) được cố định ở **5000Hz** với độ phân giải 8-bit (0-255) thông qua module `ledc` của ESP-IDF.
* **Khối Phản hồi (Feedback):** Cảm biến dòng điện INA219 (độ phân giải 0.1mA), giao tiếp qua bus I2C ở tốc độ Fast Mode 400kHz.
* **Quy hoạch nguồn (Power Topology):** Nguồn 24V cấp cho L298N được cách ly hoàn toàn với nguồn 5V cấp cho ESP32 và INA219 thông qua mạch giảm áp xung (Buck Converter), triệt tiêu hiện tượng sụt áp (Brownout Reset) khi động cơ khởi động dòng đỉnh.

**2.4. Triển khai Firmware & Chuyển đổi Logic (Firmware & Sim-to-Real Transfer)**
Logic điều khiển từ mô phỏng được chuyển ngữ sang C/C++ trên nền tảng ESP-IDF kết hợp FreeRTOS.
* **Xử lý Sim-to-Real Gap:** Quá trình tinh chỉnh vi phân (Fine-tuning) ghi nhận độ lệch giữa mô phỏng và thực tế. Nhiễu băm xung (Switching noise) từ L298N tác động lên INA219 kết hợp với chu kỳ lấy mẫu 50ms gây ra bão hòa tích phân (Integral Windup). Để khắc phục, hệ số tích phân được ép xuống mức thực nghiệm ($K_p = 1.5, K_i = 2$). Việc giảm $K_i$ đóng vai trò như bộ lọc thông thấp tĩnh, triệt tiêu dao động PWM (Chattering) mà vẫn đảm bảo triệt tiêu sai số xác lập.
* **Kiến trúc RTOS:** * **Core 1 (Control Task):** Hàm `vTaskDelayUntil` ép vòng lặp PI và đọc INA219 chạy với chu kỳ tất định chính xác **50ms**.
  * **Core 0 (SCADA Task):** Vận hành Embedded Web Server (SPA Architecture). Giao tiếp dữ liệu nội bộ qua cơ chế AJAX Polling (200ms).
  * **An toàn Đồng tranh (Concurrency Safety):** Các biến trạng thái toàn cục (`Setpoint`, `Kp`, `Ki`, `Current_mA`) được bao bọc bởi `Mutex`. Cơ chế này loại bỏ hoàn toàn xung đột đọc/ghi khi Web Server và Control Loop truy xuất dữ liệu đồng thời.

---
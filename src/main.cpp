#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "soc/rtc_cntl_reg.h"

#include "Config.h"
#include "Encoder.h"
#include "UserInput.h"
#include "MotorDrive.h"
#include "INA219_IDF.h"
#include "OLED_IDF.h"
#include "PIDControl.h"
#include "WebConfig.h"

// ================= GLOBAL OBJECTS =================
Encoder encoder(PIN_ENC);
UserInput userInput;
MotorDrive motor(PIN_IN1, PIN_IN2, PIN_ENA);
INA219_IDF ina219(I2C_NUM_0, PIN_SDA0, PIN_SCL0, 0x40);
OLED_IDF oled(I2C_NUM_1, PIN_SDA1, PIN_SCL1, 0x3C);
PIDControl pidCurrent(4.05f, 16.2f, 0.0f, 255.0f);
WebConfig webConfig;
adc_oneshot_unit_handle_t adc1_handle;

// ================= SHARED VARIABLES =================
SemaphoreHandle_t stateMutex;

uint8_t g_mode = 1; 
bool g_motor_running = false;
bool g_dir_forward = true;   
uint8_t g_tune_select = 0;   

float g_setpoint_mA = 0.0f;
float g_current_mA = 0.0f;
float g_rpm = 0.0f;
uint32_t g_pwm_out = 0;

float sys_kp = 4.05f;
float sys_ki = 16.2f;

uint8_t graph_buffer[128] = {0}; 

// ================= CALLBACK =================
void onPidUpdate(float kp, float ki) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    sys_kp = kp; sys_ki = ki;
    pidCurrent.setParams(kp, ki);
    xSemaphoreGive(stateMutex);
}

// ================= ENTRY POINT =================
void TaskControl(void *pvParameters);
void TaskHMI(void *pvParameters);

extern "C" void app_main() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Tắt cảm biến

    motor.init();
    encoder.init();
    userInput.init();
    ina219.init(); 
    oled.init();

    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN_DB_12;
    config.bitwidth = ADC_BITWIDTH_12;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));

    stateMutex = xSemaphoreCreateMutex();
    
    // CHUYỂN TOÀN BỘ SANG CORE 1 ĐỂ GIẢI PHÓNG CORE 0 CHO WI-FI
    xTaskCreatePinnedToCore(TaskControl, "Control", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskHMI,     "HMI",     8192, NULL, 2, NULL, 1);
}

// ================= TASK CONTROL (Core 1 - 50ms) =================
void TaskControl(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const float dt = 0.05f; 
    float filtered_adc = 0.0f;

    for (;;) {
        float current_mA = ina219.getCurrent_mA();

        // 1. Đọc và lọc nhiễu ADC (Biến trở)
        int adc_raw = 0; uint32_t adc_sum = 0;
        for (int i = 0; i < 10; i++) {
            adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
            adc_sum += adc_raw;
        }
        float avg_adc = adc_sum / 10.0f;
        filtered_adc = (0.2f * avg_adc) + (0.8f * filtered_adc); 

        // 2. Lấy trạng thái hệ thống từ Global (Bao gồm dữ liệu từ Web)
        uint8_t local_mode; bool is_running; bool dir_fwd; uint8_t tune_sel;
        float current_kp; float current_ki; float web_sp;
        
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        local_mode = g_mode;
        is_running = g_motor_running;
        dir_fwd = g_dir_forward;
        tune_sel = g_tune_select;
        g_current_mA = current_mA;
        current_kp = sys_kp; 
        current_ki = sys_ki;
        web_sp = g_setpoint_mA; // Lấy Setpoint do Web truyền xuống
        xSemaphoreGive(stateMutex);

        // Cập nhật thông số PID lập tức (Web chỉnh Kp/Ki)
        pidCurrent.setParams(current_kp, current_ki);

        uint32_t target_pwm = 0;

        // 3. Xử lý logic Chạy Động Cơ
        if (is_running) {
            if (local_mode == 4) { // Mode Manual
                target_pwm = (uint32_t)((filtered_adc / 4095.0f) * 255.0f);
                motor.setDirection(dir_fwd);
                motor.setPWM(target_pwm);
            } 
            else { // Các Mode PID (1, 2, 5)
                float sp;
                
                // BỘ CHUYỂN MẠCH SETPOINT (Vật lý vs Web)
                if (local_mode == 5) {
                    sp = web_sp; // Lấy từ thanh trượt trên Web
                } else {
                    sp = (float)( (int)((filtered_adc / 4095.0f) * 500.0f) ); // Lấy từ biến trở
                    
                    // Cập nhật ngược lại để hiển thị Web khớp với biến trở
                    xSemaphoreTake(stateMutex, portMAX_DELAY);
                    g_setpoint_mA = sp; 
                    xSemaphoreGive(stateMutex);
                }
                
                // CƠ CHẾ BẢO VỆ CHỐNG GIẬT SẬP NGUỒN (Deadband)
                if (sp < 5.0f) {
                    target_pwm = 0;
                    pidCurrent.reset();
                    motor.stop();
                } else {
                    bool is_sat = false;
                    float out = pidCurrent.compute(sp, current_mA, dt, is_sat);
                    target_pwm = (uint32_t)out;
                    
                    motor.setDirection(true); 
                    motor.setPWM(target_pwm);
                }
            }
        } else {
            // Động cơ bị tắt (Bằng nút hoặc Web)
            pidCurrent.reset(); 
            motor.stop();
            target_pwm = 0;
        }

        // 4. Tune PID bằng biến trở cứng (Chỉ ăn khi đang STOP và ở Mode 3)
        if (!is_running && local_mode == 3) {
            float new_kp = current_kp;
            float new_ki = current_ki;
            
            if (tune_sel == 0) new_kp = (filtered_adc / 4095.0f) * 20.0f; 
            else               new_ki = (filtered_adc / 4095.0f) * 20.0f;  
            
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            sys_kp = new_kp;
            sys_ki = new_ki;
            xSemaphoreGive(stateMutex);
        }

        // 5. Cập nhật PWM ra Global để HMI/Web đọc
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        g_pwm_out = target_pwm;
        xSemaphoreGive(stateMutex);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}

// ================= TASK HMI (Core 1 - 20ms) =================
void TaskHMI(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t ui_counter = 0; 
    bool trigger_web_start = false;
    bool trigger_web_stop = false;
    bool web_screen_drawn = false; // Cờ chống spam vẽ màn hình Web

    for (;;) {
        userInput.updateBuzzer();
        ButtonID btn = userInput.scanButtons();

        xSemaphoreTake(stateMutex, portMAX_DELAY);
        if (btn == BTN_UP) { 
            g_motor_running = !g_motor_running;
            if(!g_motor_running) pidCurrent.reset();
        }

        if (btn == BTN_LEFT || btn == BTN_RIGHT) {
            g_motor_running = false; 
            pidCurrent.reset();
            uint8_t old_mode = g_mode;

            if (btn == BTN_RIGHT) g_mode = (g_mode >= 5) ? 1 : (g_mode + 1);
            if (btn == BTN_LEFT)  g_mode = (g_mode <= 1) ? 5 : (g_mode - 1);

            if (g_mode == 5 && old_mode != 5) trigger_web_start = true;
            if (g_mode != 5 && old_mode == 5) {
                trigger_web_stop = true;
                web_screen_drawn = false; // Reset cờ vẽ khi thoát Web
            }
        }

        if (btn == BTN_DOWN) {
            if (g_mode == 3) g_tune_select = !g_tune_select; 
            if (g_mode == 4) g_dir_forward = !g_dir_forward; 
        }
        xSemaphoreGive(stateMutex);

        if (trigger_web_start) { 
            oled.clear(); oled.print("STARTING AP..."); oled.display(); 
            webConfig.startAP(); webConfig.startServer(); 
            trigger_web_start = false; 
        }
        if (trigger_web_stop)  { 
            webConfig.stopServer(); webConfig.stopAP(); 
            trigger_web_stop = false; 
        }

        ui_counter++;
        if (ui_counter >= 5) { 
            ui_counter = 0;
            uint32_t pulses = encoder.getPulsesAndReset();
            float rpm = pulses * 150.0f; 

            float cur_mA; uint8_t mode; uint8_t tune_sel; bool is_run; bool dir_fwd; float sp; uint32_t pwm;

            xSemaphoreTake(stateMutex, portMAX_DELAY);
            cur_mA = g_current_mA; mode = g_mode; tune_sel = g_tune_select; 
            is_run = g_motor_running; sp = g_setpoint_mA; pwm = g_pwm_out; dir_fwd = g_dir_forward;
            g_rpm = rpm;
            xSemaphoreGive(stateMutex);

            for(int i=0; i<127; i++) graph_buffer[i] = graph_buffer[i+1];
            int y_val = 63 - (int)((cur_mA / 500.0f) * 40.0f);

            if(y_val < 24) y_val = 24; 
            if(y_val > 63) y_val = 63;
            graph_buffer[127] = y_val;

            // XỬ LÝ VẼ MÀN HÌNH
            if (mode == 5) {
                if (!web_screen_drawn) {
                    oled.clear();
                    oled.setCursor(0, 0);
                    oled.print("--- WEB SERVER ---\n");
                    oled.print("SSID: ESP32_PID_TUNING\n");
                    oled.print("IP: 192.168.4.1\n\n");
                    oled.print("Access to config PID");
                    oled.display();
                    web_screen_drawn = true;
                }
            } else {
                oled.clear();
                oled.setCursor(0, 0);
                switch (mode) {
                    case 1: 
                        oled.print("--- NORMAL PID ---\n");
                        oled.print("Set: "); oled.print((int)sp); oled.print(" mA\n"); 
                        oled.print("Cur: "); oled.print(cur_mA, 4, 1); oled.print(" mA\n");
                        oled.print("RPM: "); oled.print(rpm, 4, 0); oled.print(" | PWM: "); oled.print((int)pwm); oled.print("\n");
                        oled.print(is_run ? "[ RUNNING ]" : "[ STOPPED ]");
                        break;
                    case 2: 
                        oled.print("--- CURRENT PLOT ---\n");
                        oled.print(cur_mA, 4, 1); oled.print(" mA");
                        for(int x=0; x<128; x++) oled.drawPixel(x, graph_buffer[x], true);
                        break;
                    case 3: 
                        oled.print("--- LIVE TUNING ---\n");
                        oled.print("Btn DOWN: Select\n");
                        if (tune_sel == 0) oled.print("> Kp: "); else oled.print("  Kp: "); oled.print(sys_kp, 4, 2); oled.print("\n");
                        if (tune_sel == 1) oled.print("> Ki: "); else oled.print("  Ki: "); oled.print(sys_ki, 4, 2); oled.print("\n");
                        oled.print("STOP to tune via POT");
                        break;
                    case 4: 
                        oled.print("--- MANUAL TEST ---\n");
                        oled.print("PWM Out: "); oled.print((int)pwm); oled.print("/255\n");
                        oled.print("Cur: "); oled.print(cur_mA, 4, 1); oled.print(" mA\n");
                        oled.print("Dir: "); oled.print(dir_fwd ? "FORWARD\n" : "REVERSE\n");
                        oled.print(is_run ? "[ RUNNING ]" : "[ STOPPED ]");
                        break;
                }
                oled.display();
            }
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20)); 
    }
}
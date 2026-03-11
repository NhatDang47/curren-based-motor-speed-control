#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#include "Config.h"
#include "MotorDrive.h"
#include "PIDControl.h"
#include "Encoder.h"
#include "INA219_IDF.h"
#include "UserInput.h"
#include "WebConfig.h"
#include "OLED_IDF.h"

// --- Global Objects ---
MotorDrive motor(PIN_IN1, PIN_IN2, PIN_ENA);
Encoder encoder(PIN_ENC);
INA219_IDF ina219(I2C_NUM_0, PIN_SDA0, PIN_SCL0);
UserInput userInput;
WebConfig webConfig;
OLED_IDF display(I2C_NUM_1, PIN_SDA1, PIN_SCL1);

// PID Params
float g_Kp = 1.2f;
float g_Ki = 0.5f;
PIDControl pidCurrent(g_Kp, g_Ki, 0.0f, 255.0f);

// --- Shared State Variables & Mutex ---
SemaphoreHandle_t stateMutex;

float g_setpoint_mA = 0.0f;
float g_current_mA = 0.0f;
float g_rpm = 0.0f;
int g_pwm_out = 0;

bool g_motor_on = false;
bool g_web_mode = false;
bool g_is_saturated = false;
uint8_t g_disp_mode = 1; // 1: Text, 2: Graph

uint8_t graph_data[128] = {0};
uint8_t graph_idx = 0;

// --- Callback updated from Web ---
void on_pid_update(float kp, float ki) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    pidCurrent.setParams(kp, ki);
    pidCurrent.reset();
    xSemaphoreGive(stateMutex);
    printf("PID Updated via Web: Kp=%.2f, Ki=%.2f\n", kp, ki);
}

// --- TASK 1: Real-time Control (50ms) - Core 1 ---
void TaskControl(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        float current_raw = ina219.getCurrent_mA();
        int pot_raw = userInput.readPotentiometer();

        xSemaphoreTake(stateMutex, portMAX_DELAY);
        g_current_mA = current_raw;
        
        // --- Deadband & Quantization (50mA Steps) ---
        float raw_setpoint = 0.0f;

        if (pot_raw > 2200) {
            motor.setDirection(true);
            raw_setpoint = ((pot_raw - 2200) / 1895.0f) * 500.0f;
        } else if (pot_raw < 1900) {
            motor.setDirection(false);
            raw_setpoint = ((1900 - pot_raw) / 1900.0f) * 500.0f;
        }

        if (raw_setpoint > 0.0f) {
            g_setpoint_mA = (float)(((int)(raw_setpoint + 2.5f) / 5) * 5);
            
            if (g_setpoint_mA > 500.0f) g_setpoint_mA = 500.0f;
        } else {
            g_setpoint_mA = 0.0f;
        }
        // ---------------------------------------------

        // PID Logic
        if (g_motor_on && !g_web_mode && g_setpoint_mA > 5.0f) {
            float pwm = pidCurrent.compute(g_setpoint_mA, g_current_mA, 0.05f, g_is_saturated);
            g_pwm_out = (int)pwm;
            motor.setPWM(g_pwm_out);
        } else {
            motor.stop();
            pidCurrent.reset();
            g_pwm_out = 0;
            g_is_saturated = false;
        }
        xSemaphoreGive(stateMutex);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}

// --- TASK 2: HMI & Web (100ms) - Core 0 ---
void TaskHMI(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    char val_buf[32];

    while (1) {
        uint32_t pulses = encoder.getPulsesAndReset();
        float raw_rpm = pulses * 150.0f; 
        
        static float filtered_rpm = 0.0f;
        filtered_rpm = (0.2f * raw_rpm) + (0.8f * filtered_rpm);

        ButtonState btns = userInput.scanButtons();

        xSemaphoreTake(stateMutex, portMAX_DELAY);
        g_rpm = filtered_rpm;

        // --- BUTTON LOGIC ---
        // Button 4: Toggle Web Mode
        if (btns.btn4) {
            g_web_mode = !g_web_mode;
            if (g_web_mode) {
                g_motor_on = false;
                motor.stop();
                webConfig.startAP();
                webConfig.startServer(&g_Kp, &g_Ki, on_pid_update);
            } else {
                webConfig.stopServer();
                webConfig.stopAP();
            }
        }

        if (!g_web_mode) {
            // Button 1: Start/Stop Motor
            if (btns.btn1) {
                g_motor_on = !g_motor_on;
                if (g_motor_on) pidCurrent.reset();
            }
            // Button 3: Toggle Display Mode
            if (btns.btn3) {
                g_disp_mode = (g_disp_mode == 1) ? 2 : 1;
                if (g_disp_mode == 2) memset(graph_data, 0, sizeof(graph_data));
            }
        }

        // --- OLED UPDATE ---
        display.clear();
        if (g_web_mode) {
            display.setCursor(0, 0);  display.print("--- AP CONFIG ---");
            display.setCursor(0, 16); display.print("SSID: ESP32_PID");
            display.setCursor(0, 32); display.print("IP: 192.168.4.1");
            display.setCursor(0, 48); display.print("MODE: TUNING...");
        } else {
            if (g_disp_mode == 1) { // Text Mode
                // Line 1: Motor Status
                display.setCursor(0, 0);  
                display.print("MOT: "); 
                display.print(g_motor_on ? (g_is_saturated ? "SAT " : "RUN ") : "OFF ");
                display.print(motor.getDirection() ? "[FWD]" : "[REV]");
                
                // Line 2: Setpoint (Fixed width)
                display.setCursor(0, 14); display.print("REF: "); 
                snprintf(val_buf, sizeof(val_buf), "%5.1f mA", g_setpoint_mA);
                display.print(val_buf);
                
                // Line 3: Actual Current (Fixed width)
                display.setCursor(0, 28); display.print("ACT: "); 
                snprintf(val_buf, sizeof(val_buf), "%5.1f mA", g_current_mA);
                display.print(val_buf);
                
                // Line 4: PWM Out
                display.setCursor(0, 42); display.print("PWM: "); 
                snprintf(val_buf, sizeof(val_buf), "%3d / 255", g_pwm_out);
                display.print(val_buf);
                
                // Line 5: RPM
                display.setCursor(0, 56); display.print("RPM: "); 
                snprintf(val_buf, sizeof(val_buf), "%5.0f", g_rpm);
                display.print(val_buf);

            } else { // Graph Mode
                display.setCursor(0, 0);
                snprintf(val_buf, sizeof(val_buf), "CUR: %5.1f mA", g_current_mA);
                display.print(val_buf);
                
                int val = (int)g_current_mA;
                if (val > 500) val = 500;
                if (val < 0) val = 0;
                graph_data[graph_idx] = 63 - ((val * 47) / 500); 
                
                for (int i = 0; i < 128; i++) {
                    int idx = (graph_idx + 1 + i) % 128;
                    if (graph_data[idx] != 0) display.drawPixel(i, graph_data[idx], true);
                }
                graph_idx = (graph_idx + 1) % 128;
            }
        }
        display.display();
        
        xSemaphoreGive(stateMutex);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main() {
    stateMutex = xSemaphoreCreateMutex();

    motor.init();
    encoder.init();
    ina219.init();
    userInput.init();
    display.init();

    xTaskCreatePinnedToCore(TaskControl, "ControlTask", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskHMI, "HMITask", 4096, NULL, 2, NULL, 0);
}
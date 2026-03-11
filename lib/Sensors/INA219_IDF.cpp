#include "INA219_IDF.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

INA219_IDF::INA219_IDF(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr) 
    : i2c_port(port), device_address(addr), sda_pin(sda), scl_pin(scl), 
      filtered_mA(0.0f), alpha(0.2f) {}

bool INA219_IDF::init() {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_pin;
    conf.scl_io_num = scl_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000; // Fast Mode 400kHz
    conf.clk_flags = 0;

    if (i2c_param_config(i2c_port, &conf) != ESP_OK) return false;
    
    i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);

    uint8_t reset_cmd[3] = {0x00, 0x80, 0x00};
    i2c_master_write_to_device(i2c_port, device_address, reset_cmd, 3, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(10)); // Chờ IC reset

    return true;
}

float INA219_IDF::getCurrent_mA() {
    uint8_t reg_addr = 0x01; // Shunt Voltage Register
    uint8_t data[2] = {0, 0};

    esp_err_t err = i2c_master_write_read_device(
        i2c_port, 
        device_address, 
        &reg_addr, 1, 
        data, 2, 
        pdMS_TO_TICKS(10)
    );

    if (err == ESP_OK) {
        int16_t raw_shunt = (int16_t)((data[0] << 8) | data[1]);
        
        float current_mA = (float)raw_shunt * 0.1f;

        if (fabs(current_mA) < 3000.0f) {
            filtered_mA = (alpha * fabs(current_mA)) + ((1.0f - alpha) * filtered_mA);
        }
    }

    return filtered_mA;
}
#pragma once

#include "driver/i2c.h"
#include "driver/gpio.h"

class INA219_IDF {
private:
    i2c_port_t i2c_port;
    uint8_t device_address;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;

    float filtered_mA;
    float alpha;

public:
    INA219_IDF(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr = 0x40);
    
    bool init();
    float getCurrent_mA(); 
};
#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"

class MotorDrive {
private:
    gpio_num_t in1, in2, ena;
    bool is_fwd;

public:
    MotorDrive(gpio_num_t pin_in1, gpio_num_t pin_in2, gpio_num_t pin_ena);
    
    void init();
    void setDirection(bool forward);
    void setPWM(uint32_t duty_8bit); // Range: 0-255
    void stop();
    
    bool getDirection() const { return is_fwd; }
};
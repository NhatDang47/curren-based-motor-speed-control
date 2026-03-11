#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

struct ButtonState {
    bool btn1;
    bool btn3;
    bool btn4;
};

class UserInput {
private:
    adc_oneshot_unit_handle_t adc_handle;
    adc_channel_t pot_channel;
    uint32_t last_scan_time;
    ButtonState btn_state;
    float filtered_adc;

public:
    UserInput();
    
    void init();
    int readPotentiometer();
    ButtonState scanButtons();
}; 
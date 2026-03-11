#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

class Encoder {
private:
    gpio_num_t enc_pin;
    volatile uint32_t pulse_count;
    volatile int64_t last_pulse_time;
    portMUX_TYPE mux;

    static void IRAM_ATTR isr_handler(void* arg);

public:
    Encoder(gpio_num_t pin);
    
    void init();
    uint32_t getPulsesAndReset();
};
#include "Encoder.h"
#include "esp_timer.h"

Encoder::Encoder(gpio_num_t pin) : enc_pin(pin), pulse_count(0), last_pulse_time(0) {
    mux = portMUX_INITIALIZER_UNLOCKED;
}

void IRAM_ATTR Encoder::isr_handler(void* arg) {
    Encoder* instance = static_cast<Encoder*>(arg);
    
    int64_t current_time = esp_timer_get_time();
    
    if (current_time - instance->last_pulse_time > 1000) {
        portENTER_CRITICAL_ISR(&(instance->mux));
        instance->pulse_count = instance->pulse_count + 1;
        instance->last_pulse_time = current_time;
        portEXIT_CRITICAL_ISR(&(instance->mux));
    }
}

void Encoder::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = (1ULL << enc_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    
    gpio_isr_handler_add(enc_pin, isr_handler, (void*)this);
}

uint32_t Encoder::getPulsesAndReset() {
    uint32_t pulses = 0;
    
    portENTER_CRITICAL(&(mux));
    pulses = pulse_count;
    pulse_count = 0;
    portEXIT_CRITICAL(&(mux));
    
    return pulses;
}
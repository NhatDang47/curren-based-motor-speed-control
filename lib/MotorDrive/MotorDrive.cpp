#include "MotorDrive.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

MotorDrive::MotorDrive(gpio_num_t pin_in1, gpio_num_t pin_in2, gpio_num_t pin_ena) 
    : in1(pin_in1), in2(pin_in2), ena(pin_ena), is_fwd(true) {}

void MotorDrive::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << in1) | (1ULL << in2);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(in1, 0);
    gpio_set_level(in2, 0);

    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num        = LEDC_TIMER_0;
    ledc_timer.duty_resolution  = LEDC_TIMER_8_BIT; // 0-255
    ledc_timer.freq_hz          = 5000;
    ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel        = LEDC_CHANNEL_0;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.intr_type      = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num       = ena;
    ledc_channel.duty           = 0;
    ledc_channel.hpoint         = 0;
    ledc_channel_config(&ledc_channel);
}

void MotorDrive::setDirection(bool forward) {
    if (is_fwd != forward) {
        stop();
        vTaskDelay(pdMS_TO_TICKS(10)); 
        is_fwd = forward;
    }
    gpio_set_level(in1, is_fwd ? 1 : 0);
    gpio_set_level(in2, is_fwd ? 0 : 1);
}

void MotorDrive::setPWM(uint32_t duty_8bit) {
    if (duty_8bit > 255) duty_8bit = 255;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_8bit);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void MotorDrive::stop() {
    gpio_set_level(in1, 0);
    gpio_set_level(in2, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
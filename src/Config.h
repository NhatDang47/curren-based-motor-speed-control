#pragma once
#include "driver/gpio.h"

// --- Motor L298N ---
constexpr gpio_num_t PIN_ENA = GPIO_NUM_13;
constexpr gpio_num_t PIN_IN1 = GPIO_NUM_14;
constexpr gpio_num_t PIN_IN2 = GPIO_NUM_27;

// --- Sensors & Feedback ---
constexpr gpio_num_t PIN_ENC = GPIO_NUM_4;
constexpr gpio_num_t PIN_POT = GPIO_NUM_34;

// --- I2C Buses ---
// I2C0: INA219
constexpr gpio_num_t PIN_SDA0 = GPIO_NUM_21;
constexpr gpio_num_t PIN_SCL0 = GPIO_NUM_19;
// I2C1: OLED SSD1306 (Đã đảo SDA=22, SCL=23)
constexpr gpio_num_t PIN_SDA1 = GPIO_NUM_22;
constexpr gpio_num_t PIN_SCL1 = GPIO_NUM_23;

// --- Buttons & Buzzer ---
constexpr gpio_num_t PIN_BTN_UP    = GPIO_NUM_26;
constexpr gpio_num_t PIN_BTN_RIGHT = GPIO_NUM_25;
constexpr gpio_num_t PIN_BTN_LEFT  = GPIO_NUM_5;
constexpr gpio_num_t PIN_BTN_DOWN  = GPIO_NUM_18;

constexpr gpio_num_t PIN_BEEP      = GPIO_NUM_32;
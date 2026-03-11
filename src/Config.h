#pragma once

#include "driver/gpio.h"

// --- Motor L298N ---
constexpr gpio_num_t PIN_IN1 = GPIO_NUM_2;
constexpr gpio_num_t PIN_IN2 = GPIO_NUM_4;
constexpr gpio_num_t PIN_ENA = GPIO_NUM_5;

// --- Sensors ---
constexpr gpio_num_t PIN_ENC = GPIO_NUM_13;
constexpr gpio_num_t PIN_POT = GPIO_NUM_34;

// --- I2C Buses ---
// I2C0: INA219
constexpr gpio_num_t PIN_SDA0 = GPIO_NUM_21;
constexpr gpio_num_t PIN_SCL0 = GPIO_NUM_22;
// I2C1: OLED SSD1306
constexpr gpio_num_t PIN_SDA1 = GPIO_NUM_18;
constexpr gpio_num_t PIN_SCL1 = GPIO_NUM_19;

// --- Buttons ---
constexpr gpio_num_t PIN_BTN_COM_14 = GPIO_NUM_14; 
constexpr gpio_num_t PIN_BTN_1      = GPIO_NUM_25;
constexpr gpio_num_t PIN_BTN_4      = GPIO_NUM_33;

constexpr gpio_num_t PIN_BTN_COM_25 = GPIO_NUM_25;
constexpr gpio_num_t PIN_BTN_3      = GPIO_NUM_26;
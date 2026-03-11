#pragma once

#include "driver/i2c.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdio.h>

class OLED_IDF {
private:
    i2c_port_t i2c_port;
    uint8_t address;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    
    uint8_t buffer[1024]; // Framebuffer 128x64
    uint8_t cursor_x, cursor_y;

    void sendCommand(uint8_t cmd);
    void sendData(uint8_t* data, size_t len);
    void writeChar(char c);

public:
    OLED_IDF(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr = 0x3C);
    
    bool init();
    void clear();
    void display();
    
    void drawPixel(int16_t x, int16_t y, bool white);
    void setCursor(uint8_t x, uint8_t y);
    
    void print(const char* str);
    void print(int val);
    void print(float val, int width = 5, int precision = 1);
};
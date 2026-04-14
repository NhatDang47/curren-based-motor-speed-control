#pragma once
#include "Config.h"
#include <stdint.h>

enum ButtonID {
    BTN_NONE = 0,
    BTN_UP,    // 26
    BTN_RIGHT, // 25
    BTN_LEFT,  // 5
    BTN_DOWN   // 18
};  

class UserInput {
private:
    ButtonID stable_btn;
    ButtonID last_btn_state;
    uint32_t debounce_timer;
    uint32_t buzzer_timeout;

public:
    UserInput();
    void init();
    ButtonID scanButtons(); 
    void beep(uint32_t duration_ms);
    void updateBuzzer();
};
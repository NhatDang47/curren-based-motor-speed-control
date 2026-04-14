#include "UserInput.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static uint32_t get_ms() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// Khởi tạo biến
UserInput::UserInput() : stable_btn(BTN_NONE), last_btn_state(BTN_NONE), debounce_timer(0), buzzer_timeout(0) {}

void UserInput::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pin_bit_mask = (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_RIGHT) | 
                           (1ULL << PIN_BTN_LEFT)  | (1ULL << PIN_BTN_DOWN);
    gpio_config(&io_conf);

    gpio_config_t buz_conf = {};
    buz_conf.intr_type = GPIO_INTR_DISABLE;
    buz_conf.mode = GPIO_MODE_OUTPUT;
    buz_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    buz_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    buz_conf.pin_bit_mask = (1ULL << PIN_BEEP);
    gpio_config(&buz_conf);

    gpio_set_level(PIN_BEEP, 0);
}

// Thuật toán Lọc nhiễu trạng thái 
ButtonID UserInput::scanButtons() {
    ButtonID current_btn = BTN_NONE;

    // Đọc trạng thái vật lý
    if (gpio_get_level(PIN_BTN_UP) == 0) current_btn = BTN_UP;
    else if (gpio_get_level(PIN_BTN_RIGHT) == 0) current_btn = BTN_RIGHT;
    else if (gpio_get_level(PIN_BTN_LEFT) == 0) current_btn = BTN_LEFT;
    else if (gpio_get_level(PIN_BTN_DOWN) == 0) current_btn = BTN_DOWN;

    uint32_t now = get_ms();

    // Nếu có sự thay đổi (dù là bấm hay nhả hay nhiễu), reset bộ đếm thời gian
    if (current_btn != last_btn_state) {
        debounce_timer = now;
        last_btn_state = current_btn;
    }

    // Nếu tín hiệu giữ nguyên không đổi trong 50ms liên tục
    if ((now - debounce_timer) > 50) { 
        if (current_btn != stable_btn) { // Và nó khác với trạng thái ổn định trước đó
            stable_btn = current_btn;
            
            // Chỉ trả về ID phím khi trạng thái ổn định là ĐANG BẤM (Khác NONE)
            if (stable_btn != BTN_NONE) {
                beep(50);
                return stable_btn;
            }
        }
    }
    
    return BTN_NONE;
}

void UserInput::beep(uint32_t duration_ms) {
    gpio_set_level(PIN_BEEP, 1);
    buzzer_timeout = get_ms() + duration_ms;
}

void UserInput::updateBuzzer() {
    if (buzzer_timeout > 0 && get_ms() > buzzer_timeout) {
        gpio_set_level(PIN_BEEP, 0);
        buzzer_timeout = 0;
    }
}
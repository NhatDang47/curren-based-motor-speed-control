#include "UserInput.h"
#include "esp_rom_sys.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

UserInput::UserInput() : last_scan_time(0), filtered_adc(-1.0f) {
    btn_state = {false, false, false};
    pot_channel = ADC_CHANNEL_6;
}

void UserInput::init() {
    // 1.GPIO 34 -> Analog (High-Z)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_DISABLE; 
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_34);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // 2. ADC Oneshot
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;
    init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
    adc_oneshot_new_unit(&init_config1, &adc_handle);

    // 3. Channel (12-bit)
    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN_DB_12; 
    config.bitwidth = ADC_BITWIDTH_12; // 12-bit
    adc_oneshot_config_channel(adc_handle, pot_channel, &config);
}

int UserInput::readPotentiometer() {
    int adc_raw = 0;
    int sum = 0;
    int valid_samples = 0;

    for(int i = 0; i < 10; i++) {
        if (adc_oneshot_read(adc_handle, pot_channel, &adc_raw) == ESP_OK) {
            sum += adc_raw;
            valid_samples++;
        }
    }

    if (valid_samples == 0) return (int)filtered_adc; 

    float avg = (float)sum / valid_samples;

    if (filtered_adc < 0.0f) {
        filtered_adc = avg;
    } else {
        filtered_adc = (0.3f * avg) + (0.7f * filtered_adc);
    }

    return (int)filtered_adc;
}

ButtonState UserInput::scanButtons() {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (current_time - last_scan_time < 200) return {false, false, false};

    btn_state = {false, false, false};

    // --- PHASE 1: Scan Button 1 (25) & 4 (33) ---
    gpio_reset_pin(GPIO_NUM_14); 
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_14, 0);

    gpio_reset_pin(GPIO_NUM_25);
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_25, GPIO_PULLUP_ONLY);
    
    gpio_reset_pin(GPIO_NUM_33);
    gpio_set_direction(GPIO_NUM_33, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_33, GPIO_PULLUP_ONLY);

    esp_rom_delay_us(500); 

    if (gpio_get_level(GPIO_NUM_25) == 0) {
        esp_rom_delay_us(1000);
        if (gpio_get_level(GPIO_NUM_25) == 0) btn_state.btn1 = true;
    }
    if (gpio_get_level(GPIO_NUM_33) == 0) {
        esp_rom_delay_us(1000);
        if (gpio_get_level(GPIO_NUM_33) == 0) btn_state.btn4 = true;
    }

    // --- PHASE 2: Scan Button 3 (26) ---
    gpio_reset_pin(GPIO_NUM_14); 
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);

    gpio_reset_pin(GPIO_NUM_25); 
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_25, 0);

    gpio_reset_pin(GPIO_NUM_26);
    gpio_set_direction(GPIO_NUM_26, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_26, GPIO_PULLUP_ONLY);

    esp_rom_delay_us(500);

    if (gpio_get_level(GPIO_NUM_26) == 0) {
        esp_rom_delay_us(1000);
        if (gpio_get_level(GPIO_NUM_26) == 0) btn_state.btn3 = true;
    }

    if (btn_state.btn1 || btn_state.btn3 || btn_state.btn4) {
        last_scan_time = current_time;
    }

    return btn_state;
}
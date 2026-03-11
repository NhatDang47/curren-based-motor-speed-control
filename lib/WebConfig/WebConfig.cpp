#include "WebConfig.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

float* WebConfig::kp_ptr = nullptr;
float* WebConfig::ki_ptr = nullptr;
ParamUpdateCallback WebConfig::on_update = nullptr;

WebConfig::WebConfig() : server(NULL) {}

void WebConfig::startAP() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "ESP32_PID_TUNING");
    wifi_config.ap.ssid_len = strlen("ESP32_PID_TUNING");
    strcpy((char*)wifi_config.ap.password, "12345678");
    wifi_config.ap.max_connection = 2;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

void WebConfig::stopAP() {
    esp_wifi_stop();
    esp_wifi_deinit();
}

esp_err_t WebConfig::root_get_handler(httpd_req_t *req) {
    char html[512];
    snprintf(html, sizeof(html),
        "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:sans-serif;} input{padding:10px; font-size:16px; width:120px;} button{padding:10px; font-size:16px;}</style></head><body>"
        "<h2>PID Current Tuning</h2>"
        "<form action='/set' method='GET'>"
        "Kp: <input type='number' step='0.01' name='kp' value='%.2f'><br><br>"
        "Ki: <input type='number' step='0.01' name='ki' value='%.2f'><br><br>"
        "<input type='submit' value='Save Parameters'>"
        "</form></body></html>",
        kp_ptr ? *kp_ptr : 0.0f,
        ki_ptr ? *ki_ptr : 0.0f
    );
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebConfig::set_get_handler(httpd_req_t *req) {
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[16];
        if (httpd_query_key_value(buf, "kp", param, sizeof(param)) == ESP_OK && kp_ptr) {
            *kp_ptr = atof(param);
        }
        if (httpd_query_key_value(buf, "ki", param, sizeof(param)) == ESP_OK && ki_ptr) {
            *ki_ptr = atof(param);
        }
        if (on_update) on_update(*kp_ptr, *ki_ptr);
    }
    
    const char* resp = "<html><body><h3>PID Updated!</h3><button onclick=\"location.href='/'\">Return</button></body></html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void WebConfig::startServer(float* kp, float* ki, ParamUpdateCallback cb) {
    kp_ptr = kp;
    ki_ptr = ki;
    on_update = cb;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t set_uri = { .uri = "/set", .method = HTTP_GET, .handler = set_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &set_uri);
    }
}

void WebConfig::stopServer() {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}
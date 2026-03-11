#pragma once

#include "esp_http_server.h"

typedef void (*ParamUpdateCallback)(float kp, float ki);

class WebConfig {
private:
    httpd_handle_t server;
    
    static float* kp_ptr;
    static float* ki_ptr;
    static ParamUpdateCallback on_update;

    static esp_err_t root_get_handler(httpd_req_t *req);
    static esp_err_t set_get_handler(httpd_req_t *req);

public:
    WebConfig();
    
    void startAP();
    void stopAP();
    void startServer(float* kp, float* ki, ParamUpdateCallback cb);
    void stopServer();
};
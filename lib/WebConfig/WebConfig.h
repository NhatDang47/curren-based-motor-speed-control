#pragma once

#include "esp_http_server.h"

class WebConfig {
private:
    httpd_handle_t server;

    static esp_err_t root_get_handler(httpd_req_t *req);
    static esp_err_t data_get_handler(httpd_req_t *req);
    static esp_err_t ctrl_get_handler(httpd_req_t *req);

public:
    WebConfig();
    
    void startAP();
    void stopAP();
    void startServer();
    void stopServer();
};
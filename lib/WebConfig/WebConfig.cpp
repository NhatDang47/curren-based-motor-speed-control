#include "WebConfig.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- GLOBAL STATE ---
extern SemaphoreHandle_t stateMutex;
extern bool g_motor_running;
extern float g_setpoint_mA;
extern float g_current_mA;
extern float g_rpm;
extern uint32_t g_pwm_out;
extern float sys_kp;
extern float sys_ki;

WebConfig::WebConfig() : server(NULL) {}

static bool wifi_core_initialized = false;

// --- GIAO DIỆN HTML/JS/CSS (Lưu thẳng vào Flash) ---
const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 PID Dashboard</title>
<style>
    body { font-family: 'Segoe UI', sans-serif; background: #f0f2f5; color: #1c1e21; margin: 0; padding: 15px; }
    .container { max-width: 900px; margin: auto; }
    .card { background: #fff; border-radius: 10px; padding: 20px; margin-bottom: 15px; box-shadow: 0 2px 5px rgba(0,0,0,0.05); }
    h2, h3 { margin-top: 0; color: #0056b3; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; text-align: center; }
    .box { padding: 15px; background: #e7f1ff; border-radius: 8px; border: 1px solid #b8daff; }
    .val { font-size: 26px; font-weight: bold; color: #0056b3; display: block; margin-top: 5px; }
    canvas { width: 100%; height: 260px; background: #fafafa; border: 1px solid #ddd; border-radius: 5px; }
    .controls { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    input[type=number], input[type=range] { width: 100%; box-sizing: border-box; padding: 10px; margin: 5px 0 15px; border: 1px solid #ccc; border-radius: 5px; font-size: 16px; }
    button { background: #28a745; color: #fff; border: none; padding: 14px; width: 100%; font-size: 18px; font-weight: bold; border-radius: 5px; cursor: pointer; transition: 0.2s; }
    button:active { transform: scale(0.98); }
    .btn-stop { background: #dc3545; }
    .btn-update { background: #007bff; }
    .legend { display: flex; justify-content: center; gap: 20px; margin-bottom: 10px; font-weight: bold; }
    .c-red { color: #dc3545; } .c-blue { color: #007bff; }
</style>
</head>
<body>
<div class="container">
    <div class="card">
        <h2>Hệ thống Điều khiển PID</h2>
        <div class="grid">
            <div class="box">Trạng thái<span id="st" class="val" style="color:#dc3545;">STOPPED</span></div>
            <div class="box">Tốc độ<span id="rpm" class="val">0 <small style="font-size:14px">RPM</small></span></div>
            <div class="box">Dòng điện<span id="cur" class="val">0.0 <small style="font-size:14px">mA</small></span></div>
            <div class="box">PWM Out<span id="pwm" class="val">0 <small style="font-size:14px">/255</small></span></div>
        </div>
    </div>
    <div class="card">
        <h3>Đồ thị Thời gian thực</h3>
        <div class="legend"><span class="c-red">■ Setpoint (mA)</span><span class="c-blue">■ Current (mA)</span></div>
        <canvas id="chart"></canvas>
    </div>
    <div class="controls">
        <div class="card">
            <h3>Điều khiển Động cơ</h3>
            <button id="btn-run" onclick="toggle()">BẬT ĐỘNG CƠ</button><br><br>
            <label>Setpoint: <span id="sp-val" style="font-weight:bold; color:#0056b3;">0</span> mA</label>
            <input type="range" id="sp-slide" min="0" max="500" value="0" oninput="updSP(this.value)" onchange="send()">
            <input type="number" id="sp-in" min="0" max="500" value="0" onchange="updSP(this.value); send()">
        </div>
        <div class="card">
            <h3>Tham số PID</h3>
            <label>Khâu Kp:</label><input type="number" step="0.01" id="kp" value="0.0">
            <label>Khâu Ki:</label><input type="number" step="0.01" id="ki" value="0.0">
            <button class="btn-update" onclick="send()">LƯU THAM SỐ</button>
        </div>
    </div>
</div>
<script>
    let run=0, sp=0, w=0, h=0;
    let c=document.getElementById('chart'), ctx=c.getContext('2d');
    let dSP=[], dCur=[];
    function resize() { c.width=c.clientWidth; c.height=c.clientHeight; w=c.width; h=c.height; }
    window.onresize = resize; resize();

    function updSP(v) { sp=v; document.getElementById('sp-slide').value=v; document.getElementById('sp-in').value=v; document.getElementById('sp-val').innerText=v; }
    function toggle() { run=1-run; send(); }
    
    function send() {
        let kp=document.getElementById('kp').value, ki=document.getElementById('ki').value;
        fetch(`/ctrl?run=${run}&sp=${sp}&kp=${kp}&ki=${ki}`);
    }

    function draw() {
        ctx.clearRect(0,0,w,h);
        if(dSP.length<2) return;
        let dx=w/100, sy=h/500;
        
        // Vẽ Grid Line
        ctx.strokeStyle='#eee'; ctx.lineWidth=1;
        for(let i=1; i<=5; i++) { ctx.beginPath(); ctx.moveTo(0, h-(i*100*sy)); ctx.lineTo(w, h-(i*100*sy)); ctx.stroke(); }

        ctx.lineWidth=2;
        ctx.beginPath(); ctx.strokeStyle='#dc3545';
        dSP.forEach((v,i) => i?ctx.lineTo(i*dx,h-v*sy):ctx.moveTo(i*dx,h-v*sy)); ctx.stroke();
        
        ctx.beginPath(); ctx.strokeStyle='#007bff';
        dCur.forEach((v,i) => i?ctx.lineTo(i*dx,h-v*sy):ctx.moveTo(i*dx,h-v*sy)); ctx.stroke();
    }

    setInterval(() => {
        fetch('/data').then(r=>r.json()).then(d => {
            document.getElementById('rpm').innerText = d.rpm;
            document.getElementById('cur').innerText = d.cur.toFixed(1);
            document.getElementById('pwm').innerText = d.pwm;
            
            if(document.activeElement.id !== 'kp') document.getElementById('kp').value = d.kp.toFixed(2);
            if(document.activeElement.id !== 'ki') document.getElementById('ki').value = d.ki.toFixed(2);
            if(document.activeElement.id !== 'sp-in' && document.activeElement.id !== 'sp-slide') updSP(d.sp);
            
            run = d.run;
            let b = document.getElementById('btn-run'), st = document.getElementById('st');
            if(run) { b.innerText="TẮT ĐỘNG CƠ"; b.className="btn-stop"; st.innerText="RUNNING"; st.style.color="#28a745"; }
            else { b.innerText="BẬT ĐỘNG CƠ"; b.className=""; st.innerText="STOPPED"; st.style.color="#dc3545"; }

            dSP.push(d.sp); dCur.push(d.cur);
            if(dSP.length>100) { dSP.shift(); dCur.shift(); }
            draw();
        });
    }, 200);
</script>
</body>
</html>
)rawliteral";

void WebConfig::startAP() {
    if (!wifi_core_initialized) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase(); nvs_flash_init();
        }
        esp_netif_init(); esp_event_loop_create_default(); esp_netif_create_default_wifi_ap();
        wifi_core_initialized = true;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "ESP32_PID_TUNING");
    wifi_config.ap.ssid_len = strlen("ESP32_PID_TUNING");
    strcpy((char*)wifi_config.ap.password, "12345678");
    wifi_config.ap.max_connection = 2; wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP); esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start(); esp_wifi_set_max_tx_power(40);
}

void WebConfig::stopAP() { esp_wifi_stop(); esp_wifi_deinit(); }

// Trả về HTML
esp_err_t WebConfig::root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Cấp Data JSON realtime
esp_err_t WebConfig::data_get_handler(httpd_req_t *req) {
    char json[128];
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    snprintf(json, sizeof(json), "{\"run\":%d,\"sp\":%.1f,\"cur\":%.1f,\"rpm\":%.0f,\"pwm\":%lu,\"kp\":%.2f,\"ki\":%.2f}",
             g_motor_running ? 1 : 0, g_setpoint_mA, g_current_mA, g_rpm, g_pwm_out, sys_kp, sys_ki);
    xSemaphoreGive(stateMutex);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Cập nhật lệnh từ Web
esp_err_t WebConfig::ctrl_get_handler(httpd_req_t *req) {
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[16];
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        if (httpd_query_key_value(buf, "run", param, sizeof(param)) == ESP_OK) g_motor_running = atoi(param);
        if (httpd_query_key_value(buf, "sp", param, sizeof(param)) == ESP_OK) g_setpoint_mA = atof(param);
        if (httpd_query_key_value(buf, "kp", param, sizeof(param)) == ESP_OK) sys_kp = atof(param);
        if (httpd_query_key_value(buf, "ki", param, sizeof(param)) == ESP_OK) sys_ki = atof(param);
        xSemaphoreGive(stateMutex);
    }
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void WebConfig::startServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.server_port = 80;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_uri_t data_uri = { .uri = "/data", .method = HTTP_GET, .handler = data_get_handler, .user_ctx = NULL };
        httpd_uri_t ctrl_uri = { .uri = "/ctrl", .method = HTTP_GET, .handler = ctrl_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &data_uri);
        httpd_register_uri_handler(server, &ctrl_uri);
    }
}

void WebConfig::stopServer() {
    if (server) { httpd_stop(server); server = NULL; }
}
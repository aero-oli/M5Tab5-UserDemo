/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <bsp/m5stack_tab5.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#define TAG "wifi"

namespace {

constexpr const char* kApSsid = "M5Tab5-UserDemo-WiFi";
constexpr const char* kApPass = "";
constexpr const char* kNvsNamespace = "wifi";
constexpr const char* kNvsSsid = "ssid";
constexpr const char* kNvsPass = "pass";
constexpr int kMaxStaConn = 4;

bool s_wifi_ready = false;
bool s_sta_netif_ready = false;
bool s_ap_netif_ready = false;
httpd_handle_t s_server = nullptr;
hal::HalBase::WifiStatus s_wifi_status;
hal::HalBase::OtaStatus s_ota_status;
TaskHandle_t s_ota_task = nullptr;

static void set_status(hal::HalBase::WifiConnectionState state, const std::string& error = "")
{
    s_wifi_status.state = state;
    s_wifi_status.lastError = error;
}

static esp_err_t root_handler(httpd_req_t* req)
{
    const char* html = "<!doctype html><html><body><h1>M5Tab5</h1><p>AP mode is running.</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_webserver()
{
    if (s_server != nullptr) {
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
        httpd_register_uri_handler(s_server, &uri);
    }
}

static void stop_webserver()
{
    if (s_server != nullptr) {
        httpd_stop(s_server);
        s_server = nullptr;
    }
}

static void event_handler(void*, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        set_status(hal::HalBase::WifiConnectionState::Failed, "Disconnected");
        s_wifi_status.ipAddress.clear();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        char ip[16];
        std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_status.ipAddress = ip;
        set_status(hal::HalBase::WifiConnectionState::Connected);
    }
}

static bool ensure_nvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret == ESP_OK;
}

static bool ensure_wifi()
{
    if (s_wifi_ready) {
        return true;
    }
    if (!ensure_nvs()) {
        set_status(hal::HalBase::WifiConnectionState::Failed, "NVS init failed");
        return false;
    }
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        set_status(hal::HalBase::WifiConnectionState::Failed, "netif init failed");
        return false;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        set_status(hal::HalBase::WifiConnectionState::Failed, "event loop init failed");
        return false;
    }
    if (!s_sta_netif_ready) {
        esp_netif_create_default_wifi_sta();
        s_sta_netif_ready = true;
    }
    if (!s_ap_netif_ready) {
        esp_netif_create_default_wifi_ap();
        s_ap_netif_ready = true;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        set_status(hal::HalBase::WifiConnectionState::Failed, "Wi-Fi init failed");
        return false;
    }
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr, nullptr);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    s_wifi_ready = true;
    return true;
}

static void save_credentials(const std::string& ssid, const std::string& password)
{
    nvs_handle_t nvs;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_str(nvs, kNvsSsid, ssid.c_str());
    nvs_set_str(nvs, kNvsPass, password.c_str());
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void erase_credentials()
{
    nvs_handle_t nvs;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_erase_key(nvs, kNvsSsid);
    nvs_erase_key(nvs, kNvsPass);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void ota_task(void* param)
{
    std::unique_ptr<std::string> url(static_cast<std::string*>(param));
    s_ota_status.state = hal::HalBase::OtaState::Downloading;
    s_ota_status.progress = 5;
    s_ota_status.lastError.clear();

    esp_http_client_config_t http_config = {};
    http_config.url = url->c_str();
    http_config.timeout_ms = 30000;
    http_config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;

    s_ota_status.progress = 20;
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        s_ota_status.state = hal::HalBase::OtaState::Complete;
        s_ota_status.progress = 100;
        s_ota_status.rebootRequired = true;
    } else {
        s_ota_status.state = hal::HalBase::OtaState::Failed;
        s_ota_status.lastError = "OTA failed";
    }
    s_ota_task = nullptr;
    vTaskDelete(nullptr);
}

}  // namespace

bool HalEsp32::wifi_init()
{
    mclog::tagInfo(TAG, "wifi init");
    return ensure_wifi();
}

void HalEsp32::wifi_set_status(WifiConnectionState state, const std::string& error)
{
    set_status(state, error);
}

void HalEsp32::setExtAntennaEnable(bool enable)
{
    _ext_antenna_enable = enable;
    s_wifi_status.extAntennaEnabled = enable;
    mclog::tagInfo(TAG, "set ext antenna enable: {}", _ext_antenna_enable);
    bsp_set_ext_antenna_enable(_ext_antenna_enable);
}

bool HalEsp32::getExtAntennaEnable()
{
    return _ext_antenna_enable;
}

void HalEsp32::startWifiAp()
{
    wifiStartAccessPoint();
}

hal::HalBase::WifiStatus HalEsp32::wifiGetStatus()
{
    s_wifi_status.extAntennaEnabled = _ext_antenna_enable;
    return s_wifi_status;
}

std::vector<hal::HalBase::WifiNetwork> HalEsp32::wifiScan()
{
    std::vector<WifiNetwork> networks;
    if (!ensure_wifi()) {
        return networks;
    }
    set_status(WifiConnectionState::Scanning);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    wifi_scan_config_t scan_config = {};
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        set_status(WifiConnectionState::Failed, "Scan failed");
        return networks;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    std::vector<wifi_ap_record_t> records(count);
    if (count > 0) {
        esp_wifi_scan_get_ap_records(&count, records.data());
    }
    for (const auto& record : records) {
        networks.push_back({reinterpret_cast<const char*>(record.ssid), record.rssi, record.authmode != WIFI_AUTH_OPEN,
                            record.primary});
    }
    set_status(WifiConnectionState::Idle);
    return networks;
}

bool HalEsp32::wifiConnect(const std::string& ssid, const std::string& password)
{
    if (!ensure_wifi()) {
        return false;
    }
    if (ssid.empty()) {
        set_status(WifiConnectionState::Failed, "SSID is required");
        return false;
    }
    wifi_config_t config = {};
    std::snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), "%s", ssid.c_str());
    std::snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), "%s", password.c_str());
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &config);
    esp_wifi_start();
    s_wifi_status.mode = WifiMode::Station;
    s_wifi_status.ssid = ssid;
    set_status(WifiConnectionState::Connecting);
    save_credentials(ssid, password);
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        set_status(WifiConnectionState::Failed, "Connect failed");
        return false;
    }
    return true;
}

void HalEsp32::wifiDisconnect()
{
    if (!ensure_wifi()) {
        return;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_wifi_status = {};
    s_wifi_status.extAntennaEnabled = _ext_antenna_enable;
}

void HalEsp32::wifiForgetCredentials()
{
    erase_credentials();
    wifiDisconnect();
    set_status(WifiConnectionState::Idle, "Saved credentials cleared");
}

void HalEsp32::wifiSetExtAntennaEnabled(bool enable)
{
    setExtAntennaEnable(enable);
}

bool HalEsp32::wifiStartAccessPoint()
{
    if (!ensure_wifi()) {
        return false;
    }
    wifi_config_t config = {};
    std::snprintf(reinterpret_cast<char*>(config.ap.ssid), sizeof(config.ap.ssid), "%s", kApSsid);
    std::snprintf(reinterpret_cast<char*>(config.ap.password), sizeof(config.ap.password), "%s", kApPass);
    config.ap.ssid_len = std::strlen(kApSsid);
    config.ap.max_connection = kMaxStaConn;
    config.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &config);
    esp_wifi_start();
    start_webserver();
    s_wifi_status.mode = WifiMode::AccessPoint;
    s_wifi_status.ssid = kApSsid;
    s_wifi_status.ipAddress = "192.168.4.1";
    set_status(WifiConnectionState::ApRunning);
    return true;
}

void HalEsp32::wifiStopAccessPoint()
{
    stop_webserver();
    esp_wifi_stop();
    s_wifi_status = {};
    s_wifi_status.extAntennaEnabled = _ext_antenna_enable;
}

hal::HalBase::OtaStatus HalEsp32::otaGetStatus()
{
    return s_ota_status;
}

bool HalEsp32::otaStartFromUrl(const std::string& url)
{
    if (s_ota_task != nullptr) {
        return false;
    }
    if (s_wifi_status.state != WifiConnectionState::Connected) {
        s_ota_status = {};
        s_ota_status.state = OtaState::Failed;
        s_ota_status.lastError = "Connect Wi-Fi before OTA";
        return false;
    }
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        s_ota_status = {};
        s_ota_status.state = OtaState::Failed;
        s_ota_status.lastError = "URL must start with http:// or https://";
        return false;
    }
    auto* copy = new std::string(url);
    s_ota_status = {};
    s_ota_status.state = OtaState::Checking;
    if (xTaskCreate(ota_task, "ota", 8192, copy, 5, &s_ota_task) != pdPASS) {
        delete copy;
        s_ota_status.state = OtaState::Failed;
        s_ota_status.lastError = "Could not start OTA task";
        return false;
    }
    return true;
}

void HalEsp32::otaCancel()
{
    if (s_ota_task != nullptr) {
        s_ota_status.lastError = "Cancel requested; waiting for OTA task";
    }
}

void HalEsp32::otaRebootToApply()
{
    if (s_ota_status.rebootRequired) {
        esp_restart();
    }
}

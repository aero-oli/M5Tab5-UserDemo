/* SPDX-License-Identifier: MIT */
#include "hal/hal_esp32.h"
#include <hal/network_state.h>
#include <hal/wifi_credentials.h>
#include <hal/ota_transfer.h>
#include <apps/runtime/async_job_runner.h>
#include <algorithm>
#include <cstring>
#include <array>
#include <memory>
#include <bsp/m5stack_tab5.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_netif.h>
#include <esp_wifi_default.h>
#include <esp_wifi_netif.h>
#include <esp_private/wifi.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE || CONFIG_ESP_TLS_INSECURE
#error "OTA requires the verified default TLS certificate bundle"
#endif

namespace {
using State = hal::HalBase::WifiConnectionState;
using Mode = hal::HalBase::WifiMode;
using OtaState = hal::HalBase::OtaState;
hal::NetworkState state;
std::atomic<bool> commandBusy {false};
std::atomic<uint64_t> stoppedEvents {0};
std::atomic<uint64_t> stoppedApEvents {0};
app_runtime::AsyncJobRunner otaRunner;
app_runtime::AsyncJobRunner restoreRunner;
bool ready = false;
bool started = false; // Command lease owns all SDK configuration/server access.
bool startedAp = false;
bool stopPending = false;
uint64_t stopBoundary = 0;
bool stopWasAp = false;
esp_netif_t* staNetif = nullptr;
esp_netif_t* apNetif = nullptr;
httpd_handle_t server = nullptr;
esp_event_handler_instance_t wifiHandler = nullptr;
esp_event_handler_instance_t ipHandler = nullptr;
esp_event_handler_instance_t apStartHandler = nullptr;
esp_event_handler_instance_t apStopHandler = nullptr;
// Only the event-loop owner touches this flag. Commands communicate through atomics.
bool apNetifStarted = false;
std::atomic<bool> apWanted {false};
std::atomic<bool> apStopExpected {false};
std::atomic<int> apStartResult {0}; // 0 pending, 1 ready, -1 failed.
constexpr const char* apSsid = "M5Tab5-UserDemo-WiFi";
uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void pauseMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void wifiError(const std::string& message)
{
    state.update([&](auto& value) { value.wifi.lastError = message; });
}
struct WifiCommandResult {
    bool success = false;
    ~WifiCommandResult()
    {
        if (!success) state.update([](auto& value) {
            value.stationWanted = false; ++value.wifiGeneration;
            value.wifi.state = State::Failed;
            value.wifi.ipAddress.clear();
        });
    }
};
bool checkWifi(esp_err_t result, const char* action)
{
    if (result == ESP_OK) return true;
    wifiError(std::string(action) + ": " + esp_err_to_name(result));
    return false;
}
// Updated C6 firmware may repeat AP events. Never invoke the netif start/stop
// action twice: the default AP start action is not idempotent.
void remoteApStart(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* netif = static_cast<esp_netif_t*>(arg);
    if (!apWanted.load() || apNetifStarted || esp_netif_is_netif_up(netif)) return;
    apNetifStarted = true; // Also retain ownership if any subsequent setup fails.
    auto fail = [](esp_err_t result, const char* operation) {
        if (checkWifi(result, operation)) return false;
        apStartResult.store(-1);
        return true;
    };
    auto driver = static_cast<wifi_netif_driver_t>(esp_netif_get_io_driver(netif));
    uint8_t mac[6];
    if (fail(esp_wifi_get_if_mac(driver, mac), "AP MAC read failed")) return;
    if (esp_wifi_is_if_ready_when_started(driver) &&
        fail(esp_wifi_register_if_rxcb(driver, esp_netif_receive, netif), "AP receive registration failed")) return;
    if (fail(esp_wifi_internal_reg_netstack_buf_cb(esp_netif_netstack_buf_ref, esp_netif_netstack_buf_free),
             "AP network buffer registration failed") ||
        fail(esp_netif_set_mac(netif, mac), "AP MAC setup failed")) return;
    esp_netif_action_start(netif, base, id, data);
    if (!esp_netif_is_netif_up(netif)) {
        wifiError("AP network interface did not start");
        apStartResult.store(-1);
        return;
    }
    apStartResult.store(1);
}
void remoteApStop(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* netif = static_cast<esp_netif_t*>(arg);
    if (!apStopExpected.load() && !apNetifStarted && !esp_netif_is_netif_up(netif)) return;
    if (apNetifStarted || esp_netif_is_netif_up(netif)) {
        esp_netif_action_stop(netif, base, id, data);
        if (esp_netif_is_netif_up(netif)) {
            wifiError("AP network interface did not stop");
            return; // Retain ownership and the pending transition boundary.
        }
    }
    apNetifStarted = false;
    if (apStopExpected.exchange(false)) {
        stoppedApEvents.fetch_add(1); // Publish only after accepted netif teardown.
    } else {
        apWanted.store(false);
        state.update([](auto& value) {
            if (value.wifi.mode != Mode::AccessPoint) return;
            value.wifi.state = State::Failed;
            value.wifi.ipAddress.clear();
            value.wifi.lastError = "Access point stopped unexpectedly";
        });
    }
}
bool ensureRemoteApNetif()
{
    if (!apNetif) {
        esp_netif_config_t config = ESP_NETIF_DEFAULT_WIFI_AP();
        auto* netif = esp_netif_new(&config);
        if (!netif) { wifiError("Could not allocate AP interface"); return false; }
        // The default STA constructor registers AP handlers too. Avoid the SDK's
        // global default AP pointer so those handlers cannot bypass our guards.
        auto driver = esp_wifi_create_if_driver(WIFI_IF_AP);
        if (!driver) { esp_netif_destroy(netif); wifiError("Could not allocate AP driver"); return false; }
        if (!checkWifi(esp_netif_attach(netif, driver), "AP driver attachment failed")) {
            esp_wifi_destroy_if_driver(driver);
            esp_netif_destroy(netif);
            return false;
        }
        apNetif = netif;
    }
    // Retain successful registrations on failure; retries cannot duplicate handlers.
    if (!apStartHandler && !checkWifi(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START,
            remoteApStart, apNetif, &apStartHandler), "AP start registration failed")) return false;
    if (!apStopHandler && !checkWifi(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STOP,
            remoteApStop, apNetif, &apStopHandler), "AP stop registration failed")) return false;
    return true;
}
void setIdle()
{
    state.update([](auto& value) {
        const bool antenna = value.wifi.extAntennaEnabled;
        value.stationWanted = false;
        ++value.wifiGeneration;
        value.wifi = {};
        value.wifi.extAntennaEnabled = antenna;
    });
}
std::string apName(const wifi_ap_record_t& ap)
{
    return std::string(reinterpret_cast<const char*>(ap.ssid), strnlen(reinterpret_cast<const char*>(ap.ssid), sizeof(ap.ssid)));
}
void eventHandler(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) {
        stoppedEvents.fetch_add(1);
        return;
    }
    const auto before = state.snapshot();
    if (!before.stationWanted || before.wifi.mode != Mode::Station) return;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto* event = static_cast<wifi_event_sta_disconnected_t*>(data);
        if (event->ssid_len > sizeof(event->ssid)) return;
        if (std::string(reinterpret_cast<const char*>(event->ssid), event->ssid_len) != before.wifi.ssid) return;
        // A queued disconnect must not erase an already established new link.
        wifi_ap_record_t actual {};
        if (esp_wifi_sta_get_ap_info(&actual) == ESP_OK && apName(actual) == before.wifi.ssid) return;
        state.stationDisconnected(before.wifiGeneration, before.wifi.ssid,
                                  "Disconnected (reason " + std::to_string(event->reason) + ")");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        esp_netif_ip_info_t address {};
        if (esp_netif_get_ip_info(staNetif, &address) == ESP_OK && address.ip.addr != 0) return;
        state.stationDisconnected(before.wifiGeneration, before.wifi.ssid, "Station lost its IP address");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        wifi_ap_record_t actual {};
        esp_netif_ip_info_t address {};
        if (event->esp_netif != staNetif || esp_wifi_sta_get_ap_info(&actual) != ESP_OK ||
            apName(actual) != before.wifi.ssid || esp_netif_get_ip_info(staNetif, &address) != ESP_OK ||
            address.ip.addr != event->ip_info.ip.addr) return;
        char ip[16];
        std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&address.ip));
        state.stationConnected(before.wifiGeneration, before.wifi.ssid, ip, actual.rssi);
    }
}
bool ensureNvs()
{
    return checkWifi(nvs_flash_init(), "NVS initialization failed"); // Never erase unrelated saved configuration.
}
bool ensureWifi()
{
    if (ready) return true;
    if (!ensureNvs()) return false;
    auto result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return checkWifi(result, "Network initialization failed");
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return checkWifi(result, "Event loop initialization failed");
    if (!staNetif) staNetif = esp_netif_create_default_wifi_sta();
    if (!ensureRemoteApNetif()) return false;
    if (!staNetif || !apNetif) { wifiError("Could not create Wi-Fi interfaces"); return false; }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK && result != ESP_ERR_WIFI_INIT_STATE) return checkWifi(result, "Wi-Fi initialization failed");
    if (!wifiHandler && !checkWifi(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler, nullptr, &wifiHandler),
                                  "Wi-Fi event registration failed")) return false;
    if (!ipHandler && !checkWifi(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, eventHandler, nullptr, &ipHandler),
                                "IP event registration failed")) return false;
    if (!checkWifi(esp_wifi_set_storage(WIFI_STORAGE_RAM), "Wi-Fi storage setup failed")) return false;
    ready = true;
    return true;
}
void stopServer()
{
    if (server) { httpd_stop(server); server = nullptr; }
}
esp_err_t rootHandler(httpd_req_t* request)
{
    return httpd_resp_send(request, "<!doctype html><html><body><h1>M5Tab5</h1><p>AP mode is running.</p></body></html>", HTTPD_RESP_USE_STRLEN);
}
bool startServer()
{
    if (server) return true;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (!checkWifi(httpd_start(&server, &config), "AP server start failed")) return false;
    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = rootHandler, .user_ctx = nullptr};
    if (checkWifi(httpd_register_uri_handler(server, &root), "AP server registration failed")) return true;
    stopServer();
    return false;
}
bool stopForTransition()
{
    state.stopStationIntent();
    stopServer();
    if (started) {
        stopWasAp = startedAp;
        apWanted.store(false);
        stopBoundary = stopWasAp ? stoppedApEvents.load() : stoppedEvents.load();
        apStopExpected.store(stopWasAp);
        if (!checkWifi(esp_wifi_stop(), "Wi-Fi stop failed")) return false;
        started = false;
        stopPending = true;
    }
    if (!stopPending) return true;
    const auto began = nowMs();
    // Driver STOP and default-netif teardown form a boundary before a new intent.
    // Keep the pending boundary on timeout: a retry cannot start over stale STOP.
    const auto stoppedCount = [] { return stopWasAp ? stoppedApEvents.load() : stoppedEvents.load(); };
    while (stoppedCount() == stopBoundary && nowMs() - began < 2000) pauseMs(5);
    if (stoppedCount() == stopBoundary) { wifiError("Wi-Fi stop event timed out"); return false; }
    stopPending = false;
    startedAp = false;
    apStartResult.store(0);
    return true;
}
bool saveCredentials(const std::string& ssid, const std::string& password)
{
    nvs_handle_t handle;
    auto result = nvs_open("wifi", NVS_READWRITE, &handle);
    if (result != ESP_OK) return checkWifi(result, "Saved credentials unavailable");
    result = nvs_set_str(handle, "ssid", ssid.c_str());
    if (result == ESP_OK) result = nvs_set_str(handle, "pass", password.c_str());
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return checkWifi(result, "Could not save credentials");
}
bool connectStation(const std::string& ssid, const std::string& password, bool save)
{
    hal::WifiCredentialBytes bytes;
    if (!hal::encodeWifiCredentials(ssid, password, bytes)) { wifiError("Invalid SSID or password length/format"); return false; }
    WifiCommandResult outcome;
    if (!ensureWifi() || !stopForTransition()) return false;
    wifi_config_t config {};
    std::copy(bytes.ssid.begin(), bytes.ssid.end(), config.sta.ssid);
    std::copy(bytes.password.begin(), bytes.password.end(), config.sta.password);
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    if (!checkWifi(esp_wifi_set_mode(WIFI_MODE_STA), "Station mode failed") ||
        !checkWifi(esp_wifi_set_config(WIFI_IF_STA, &config), "Station configuration failed")) return false;
    state.beginStation(ssid, nowMs());
    if (!checkWifi(esp_wifi_start(), "Wi-Fi start failed")) return false;
    started = true;
    if (!checkWifi(esp_wifi_connect(), "Connect failed")) {
        state.update([](auto& value) { value.stationWanted = false; value.wifi.state = State::Failed; });
        return false;
    }
    if (save) saveCredentials(ssid, password); // Save only after the driver accepted this attempt.
    outcome.success = true;
    return true;
}

void runOta(const std::string& url)
{
    static_assert(sizeof(esp_image_header_t) == 24 && offsetof(esp_image_header_t, chip_id) == 12);
    static_assert(sizeof(esp_image_segment_header_t) == 8 && sizeof(esp_app_desc_t) == 256 && offsetof(esp_app_desc_t, project_name) == 48);
    esp_http_client_handle_t client = nullptr;
    esp_ota_handle_t image = 0;
    bool imageOpen = false;
    std::string detail;
    const auto* partition = esp_ota_get_next_update_partition(nullptr);
    const auto* running = esp_ota_get_running_partition();
    const bool usable = partition && running && partition->address != running->address && partition->type == ESP_PARTITION_TYPE_APP;
    const auto began = nowMs();
    auto checked = [&](esp_err_t code, const char* operation, std::string& error) {
        if (code == ESP_OK) return true;
        error = std::string(operation) + ": " + esp_err_to_name(code);
        return false;
    };
    hal::OtaTransferCallbacks io;
    io.cancelled = [] { return state.otaCancelled(); };
    io.open = [&](const std::string&, auto& info, std::string& error) {
        esp_http_client_config_t config {};
        config.url = url.c_str();
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = 5000;
        config.buffer_size = 4096;
        config.disable_auto_redirect = true;
        config.transport_type = HTTP_TRANSPORT_OVER_SSL;
        client = esp_http_client_init(&config);
        if (!client) { error = "Could not allocate HTTPS client"; return false; }
        if (!checked(esp_http_client_open(client, 0), "HTTPS connection failed; check certificate trust and RTC clock", error)) return false;
        const auto length = esp_http_client_fetch_headers(client);
        if (length < 0) { error = "Could not read firmware HTTP headers"; return false; }
        info.httpStatus = esp_http_client_get_status_code(client);
        info.contentLength = length > 0 ? length : -1;
        state.update([](auto& value) { if (!value.otaCancelRequested) value.ota.state = OtaState::Downloading; });
        return true;
    };
    io.read = [&](uint8_t* data, size_t size) {
        if (nowMs() - began > 300000) { detail = "Firmware download exceeded five-minute deadline"; return -1; }
        return esp_http_client_read(client, reinterpret_cast<char*>(data), size);
    };
    io.bodyComplete = [&] { return esp_http_client_is_complete_data_received(client); };
    io.validateHeader = [&](const uint8_t* data, size_t length, std::string& error) {
        return hal::validOtaImageHeader(data, length, CONFIG_IDF_FIRMWARE_CHIP_ID, esp_app_get_description()->project_name, error);
    };
    io.beginImage = [&](size_t, std::string& error) {
        imageOpen = checked(esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &image), "Could not open OTA partition", error);
        return imageOpen;
    };
    io.writeImage = [&](const uint8_t* data, size_t length, std::string& error) {
        return checked(esp_ota_write(image, data, length), "Firmware write failed", error);
    };
    io.validateImage = [&](std::string& error) {
        imageOpen = false; // esp_ota_end invalidates its handle on both success and failure.
        return checked(esp_ota_end(image), "Firmware image validation failed", error);
    };
    io.enterCommit = [] { return state.enterOtaCommit(); };
    io.selectBoot = [&](std::string& error) { return checked(esp_ota_set_boot_partition(partition), "Could not select new firmware", error); };
    io.cleanup = [&] {
        if (imageOpen) { checked(esp_ota_abort(image), "OTA cleanup failed", detail); imageOpen = false; }
        if (client) {
            checked(esp_http_client_close(client), "HTTPS close failed", detail);
            checked(esp_http_client_cleanup(client), "HTTPS cleanup failed", detail);
            client = nullptr;
        }
    };
    io.progress = [](size_t received, size_t total, bool known) {
        state.update([&](auto& value) {
            value.ota.bytesReceived = received;
            value.ota.totalBytes = total;
            value.ota.progressKnown = known;
            value.ota.progress = known ? std::min(99, static_cast<int>(received * 100ULL / total)) : 0;
        });
    };
    io.installing = [] { state.update([](auto& value) { if (!value.otaCancelRequested) value.ota.state = OtaState::Installing; }); };
    auto result = hal::runOtaTransfer(url, usable ? partition->size : 0, io);
    state.update([&](auto& value) {
        value.otaActive = false;
        value.ota.canCancel = false;
        value.ota.state = result.outcome == hal::OtaTransferOutcome::Complete ? OtaState::Complete :
                          result.outcome == hal::OtaTransferOutcome::Cancelled ? OtaState::Cancelled : OtaState::Failed;
        value.ota.lastError = result.error;
        if (!detail.empty()) value.ota.lastError += (value.ota.lastError.empty() ? "" : "; ") + detail;
        value.ota.rebootRequired = result.outcome == hal::OtaTransferOutcome::Complete;
        if (value.ota.rebootRequired) { value.ota.bytesReceived = result.bytesReceived; value.ota.totalBytes = result.bytesReceived; value.ota.progressKnown = true; value.ota.progress = 100; }
    });
}
} // namespace

bool HalEsp32::wifi_init() { return wifiRestoreSavedConnection(); }
void HalEsp32::wifi_set_status(WifiConnectionState value, const std::string& error)
{
    state.update([&](auto& data) { data.wifi.state = value; data.wifi.lastError = error; });
}
bool HalEsp32::wifiRestoreSavedConnection()
{
    auto lease = std::make_shared<hal::NetworkCommandLease>(commandBusy);
    if (!*lease || !state.allowsCommands()) return false;
    return restoreRunner.start("Restore Wi-Fi", [lease] {
        if (!state.allowsCommands() || !ensureWifi()) return;
        nvs_handle_t handle;
        const auto opened = nvs_open("wifi", NVS_READONLY, &handle);
        if (opened == ESP_ERR_NVS_NOT_FOUND) return;
        if (!checkWifi(opened, "Could not load saved Wi-Fi")) return;
        std::array<char, 33> ssid {};
        std::array<char, 65> password {};
        size_t ssidLength = ssid.size(), passwordLength = password.size();
        const auto ssidResult = nvs_get_str(handle, "ssid", ssid.data(), &ssidLength);
        const auto passResult = nvs_get_str(handle, "pass", password.data(), &passwordLength);
        nvs_close(handle);
        if (ssidResult == ESP_ERR_NVS_NOT_FOUND && passResult == ESP_ERR_NVS_NOT_FOUND) return;
        if (ssidResult != ESP_OK || passResult != ESP_OK || ssidLength < 2 || ssidLength > ssid.size() ||
            passwordLength < 1 || passwordLength > password.size() || ssidLength != strnlen(ssid.data(), ssid.size()) + 1 ||
            passwordLength != strnlen(password.data(), password.size()) + 1) { wifiError("Saved Wi-Fi credentials are invalid"); return; }
        connectStation(ssid.data(), password.data(), false);
    });
}
bool HalEsp32::stopNetworkAndWait(uint32_t timeoutMs)
{
    state.quiesce();
    const auto began = nowMs();
    while (commandBusy.load() || otaRunner.busy() || restoreRunner.busy() || state.snapshot().otaActive) {
        if (nowMs() - began >= timeoutMs) return false;
        pauseMs(5);
    }
    return true;
}
void HalEsp32::resumeNetwork() { state.resume(); }
void HalEsp32::setExtAntennaEnable(bool enabled)
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return;
    bsp_set_ext_antenna_enable(enabled);
    state.update([&](auto& value) { value.wifi.extAntennaEnabled = enabled; });
}
bool HalEsp32::getExtAntennaEnable() { return state.snapshot().wifi.extAntennaEnabled; }
void HalEsp32::wifiSetExtAntennaEnabled(bool enabled) { setExtAntennaEnable(enabled); }
void HalEsp32::startWifiAp() { wifiStartAccessPoint(); }
hal::HalBase::WifiStatus HalEsp32::wifiGetStatus()
{
    state.expireStation(nowMs(), 20000); // Cache-only timeout; late events cannot revive an expired intent.
    return state.snapshot().wifi;
}
std::vector<hal::HalBase::WifiNetwork> HalEsp32::wifiScan()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands() || !ensureWifi()) return {};
    const auto before = state.snapshot();
    if (before.wifi.mode == Mode::AccessPoint || before.wifi.state == State::Connecting) { wifiError("Stop AP or wait for connection before scanning"); return {}; }
    const auto scanGeneration = state.beginScan();
    struct ScanResult {
        uint64_t generation;
        bool success = false;
        ~ScanResult() { state.finishScan(generation, success); }
    } outcome {scanGeneration};
    if (!checkWifi(esp_wifi_set_mode(WIFI_MODE_STA), "Scan station mode failed")) return {};
    if (!started) { if (!checkWifi(esp_wifi_start(), "Scan start failed")) return {}; started = true; }
    wifi_scan_config_t config {};
    if (!checkWifi(esp_wifi_scan_start(&config, true), "Wi-Fi scan failed")) {
        state.finishScan(scanGeneration, false);
        return {};
    }
    uint16_t count = 0;
    if (!checkWifi(esp_wifi_scan_get_ap_num(&count), "Scan count failed")) return {};
    count = std::min<uint16_t>(count, 64);
    std::vector<wifi_ap_record_t> records(count);
    if (count && !checkWifi(esp_wifi_scan_get_ap_records(&count, records.data()), "Scan results failed")) return {};
    std::vector<WifiNetwork> networks;
    for (size_t i = 0; i < count; ++i) networks.push_back({apName(records[i]), records[i].rssi, records[i].authmode != WIFI_AUTH_OPEN, records[i].primary});
    if (before.wifi.state == State::Connected) {
        wifi_ap_record_t actual {};
        esp_netif_ip_info_t address {};
        if (esp_wifi_sta_get_ap_info(&actual) == ESP_OK && apName(actual) == before.wifi.ssid &&
            esp_netif_get_ip_info(staNetif, &address) == ESP_OK && address.ip.addr != 0) {
            char ip[16];
            std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&address.ip));
            state.stationConnected(before.wifiGeneration, before.wifi.ssid, ip, actual.rssi);
        } else state.stationDisconnected(before.wifiGeneration, before.wifi.ssid, "Station link unavailable after scan");
    }
    outcome.success = true;
    return networks;
}
bool HalEsp32::wifiConnect(const std::string& ssid, const std::string& password)
{
    hal::NetworkCommandLease lease(commandBusy);
    return lease && state.allowsCommands() && connectStation(ssid, password, true);
}
void HalEsp32::wifiDisconnect()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands() || !ensureWifi()) return;
    WifiCommandResult outcome;
    if (stopForTransition()) { setIdle(); outcome.success = true; }
}
void HalEsp32::wifiForgetCredentials()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands() || !ensureWifi()) return;
    WifiCommandResult outcome;
    if (!stopForTransition()) return;
    setIdle();
    outcome.success = true;
    nvs_handle_t handle;
    if (!checkWifi(nvs_open("wifi", NVS_READWRITE, &handle), "Saved credentials unavailable")) return;
    auto result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (checkWifi(result, "Could not clear saved credentials")) wifiError("Saved credentials cleared");
}
bool HalEsp32::wifiStartAccessPoint()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return false;
    WifiCommandResult outcome;
    if (!ensureWifi() || !stopForTransition()) return false;
    wifi_config_t config {};
    memcpy(config.ap.ssid, apSsid, strlen(apSsid));
    config.ap.ssid_len = strlen(apSsid);
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;
    if (!checkWifi(esp_wifi_set_mode(WIFI_MODE_AP), "AP mode failed") ||
        !checkWifi(esp_wifi_set_config(WIFI_IF_AP, &config), "AP configuration failed")) return false;
    apStartResult.store(0);
    apWanted.store(true);
    apStopExpected.store(false);
    if (!checkWifi(esp_wifi_start(), "AP start failed")) {
        apWanted.store(false);
        apStopExpected.store(false);
        return false;
    }
    started = true;
    startedAp = true;
    const auto began = nowMs();
    while (apStartResult.load() == 0 && nowMs() - began < 2000) pauseMs(5);
    if (apStartResult.load() != 1) {
        if (apStartResult.load() == 0) wifiError("AP start event timed out");
        stopForTransition();
        return false;
    }
    if (!startServer()) { stopForTransition(); return false; }
    bool published = false;
    state.update([&](auto& value) {
        // STOP clears intent before taking this same snapshot lock. Either we
        // observe it here, or its following update invalidates our AP state.
        if (!apWanted.load()) {
            value.wifi.state = State::Failed;
            value.wifi.ipAddress.clear();
            value.wifi.lastError = "AP stopped during startup";
            return;
        }
        value.wifi.mode = Mode::AccessPoint; value.wifi.state = State::ApRunning;
        value.wifi.ssid = apSsid; value.wifi.ipAddress = "192.168.4.1"; value.wifi.lastError.clear();
        published = true;
    });
    if (!published) { stopForTransition(); return false; }
    outcome.success = true;
    return true;
}
void HalEsp32::wifiStopAccessPoint() { wifiDisconnect(); }
hal::HalBase::OtaStatus HalEsp32::otaGetStatus() { return state.snapshot().ota; }
bool HalEsp32::otaStartFromUrl(const std::string& url)
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease) return false;
    if (!hal::validOtaUrl(url)) {
        state.update([](auto& value) { if (!value.otaActive) { value.ota.lastError = "Use an HTTPS URL without credentials"; value.ota.state = OtaState::Failed; } });
        return false;
    }
    if (!state.reserveOta()) return false;
    if (!otaRunner.start("Firmware update", [url] { runOta(url); })) {
        state.update([](auto& value) { value.otaActive = false; value.ota.canCancel = false; value.ota.state = OtaState::Failed; value.ota.lastError = "Could not start OTA worker"; });
        return false;
    }
    return true;
}
void HalEsp32::otaCancel() { state.cancelOta(); }
void HalEsp32::otaRebootToApply()
{
    hal::NetworkCommandLease lease(commandBusy);
    const auto before = state.snapshot();
    if (lease && before.accepting && !before.otaActive && before.ota.rebootRequired) esp_restart();
}

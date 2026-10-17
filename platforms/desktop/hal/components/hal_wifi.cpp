/* SPDX-License-Identifier: MIT */
#include "../hal_desktop.h"
#include <hal/network_state.h>
#include <hal/wifi_credentials.h>
#include <hal/ota_transfer.h>
#include <apps/runtime/async_job_runner.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <cstdlib>

namespace {
using State = hal::HalBase::WifiConnectionState;
using Mode = hal::HalBase::WifiMode;
using OtaState = hal::HalBase::OtaState;
hal::NetworkState state;
std::atomic<bool> commandBusy {false};
app_runtime::AsyncJobRunner otaRunner;
bool env(const char* name) { const auto* value = std::getenv(name); return value && value[0] == '1'; }
void pauseMs(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void idle()
{
    state.update([](auto& value) {
        const bool antenna = value.wifi.extAntennaEnabled;
        value.wifi = {};
        value.wifi.extAntennaEnabled = antenna;
        value.stationWanted = false;
        ++value.wifiGeneration;
    });
}
void simulateOta(const std::string& url)
{
    constexpr size_t length = 256 * 1024;
    size_t offset = 0;
    std::array<uint8_t, hal::kOtaHeaderBytes> header {};
    header[0] = 0xe9; header[1] = 1; header[12] = 0x12;
    header[32] = 0x32; header[33] = 0x54; header[34] = 0xcd; header[35] = 0xab;
    memcpy(header.data() + 80, "m5stack_tab5", 12);
    hal::OtaTransferCallbacks io;
    io.cancelled = [] { return state.otaCancelled(); };
    io.open = [&](const std::string&, auto& info, std::string& error) {
        pauseMs(100);
        if (env("M5TAB5_DESKTOP_OTA_FAIL")) { error = "Simulated HTTPS download failure"; return false; }
        info.httpStatus = 200;
        info.contentLength = env("M5TAB5_DESKTOP_OTA_UNKNOWN_LENGTH") ? -1 : static_cast<int64_t>(length);
        state.update([](auto& value) { if (!value.otaCancelRequested) value.ota.state = OtaState::Downloading; });
        return true;
    };
    io.read = [&](uint8_t* buffer, size_t maximum) {
        pauseMs(16);
        const auto count = std::min(maximum, length - offset);
        memset(buffer, 0, count);
        for (size_t i = 0; i < count && offset + i < header.size(); ++i) buffer[i] = header[offset + i];
        offset += count;
        return static_cast<int>(count);
    };
    io.bodyComplete = [&] { return offset == length; };
    io.validateHeader = [](const uint8_t* data, size_t size, std::string& error) {
        return hal::validOtaImageHeader(data, size, 0x12, "m5stack_tab5", error);
    };
    io.beginImage = [](size_t, std::string&) { return true; };
    io.writeImage = [](const uint8_t*, size_t, std::string&) { return true; };
    io.validateImage = [](std::string&) { pauseMs(100); return true; };
    io.enterCommit = [] { return state.enterOtaCommit(); };
    io.selectBoot = [](std::string&) { pauseMs(20); return true; };
    io.cleanup = [] {};
    io.installing = [] { state.update([](auto& value) { if (!value.otaCancelRequested) value.ota.state = OtaState::Installing; }); };
    io.progress = [](size_t received, size_t total, bool known) {
        state.update([&](auto& value) {
            value.ota.bytesReceived = received; value.ota.totalBytes = total; value.ota.progressKnown = known;
            value.ota.progress = known ? std::min(99, static_cast<int>(received * 100ULL / total)) : 0;
        });
    };
    const auto result = hal::runOtaTransfer(url, 4 * 1024 * 1024, io);
    state.update([&](auto& value) {
        value.otaActive = false; value.ota.canCancel = false; value.ota.simulated = true;
        value.ota.state = result.outcome == hal::OtaTransferOutcome::Complete ? OtaState::Complete :
                          result.outcome == hal::OtaTransferOutcome::Cancelled ? OtaState::Cancelled : OtaState::Failed;
        value.ota.lastError = result.error;
        value.ota.rebootRequired = result.outcome == hal::OtaTransferOutcome::Complete;
        if (value.ota.rebootRequired) { value.ota.bytesReceived = result.bytesReceived; value.ota.totalBytes = result.bytesReceived; value.ota.progress = 100; value.ota.progressKnown = true; }
    });
}
}
void HalDesktop::setExtAntennaEnable(bool enabled)
{
    hal::NetworkCommandLease lease(commandBusy);
    if (lease && state.allowsCommands()) state.update([&](auto& value) { value.wifi.extAntennaEnabled = enabled; });
}
bool HalDesktop::getExtAntennaEnable() { return state.snapshot().wifi.extAntennaEnabled; }
void HalDesktop::wifiSetExtAntennaEnabled(bool enabled) { setExtAntennaEnable(enabled); }
hal::HalBase::WifiStatus HalDesktop::wifiGetStatus() { return state.snapshot().wifi; }
bool HalDesktop::wifiRestoreSavedConnection() { return state.allowsCommands(); } // No host network configuration is changed.
void HalDesktop::resumeNetwork() { state.resume(); }
bool HalDesktop::stopNetworkAndWait(uint32_t timeoutMs)
{
    state.quiesce();
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (commandBusy.load() || otaRunner.busy() || state.snapshot().otaActive) {
        if (std::chrono::steady_clock::now() >= end) return false;
        pauseMs(5);
    }
    return true;
}
std::vector<hal::HalBase::WifiNetwork> HalDesktop::wifiScan()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return {};
    const auto generation = state.beginScan();
    pauseMs(100);
    state.finishScan(generation, true);
    if (env("M5TAB5_DESKTOP_WIFI_NO_NETWORKS")) return {};
    return {{"Workshop", -42, true, 6}, {"M5Lab", -58, true, 11}, {"Guest", -76, false, 1}};
}
bool HalDesktop::wifiConnect(const std::string& ssid, const std::string& password)
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return false;
    hal::WifiCredentialBytes encoded;
    const bool valid = hal::encodeWifiCredentials(ssid, password, encoded);
    const bool failed = env("M5TAB5_DESKTOP_WIFI_CONNECT_FAIL");
    state.update([&](auto& value) {
        ++value.wifiGeneration; value.stationWanted = valid && !failed;
        value.wifi.mode = Mode::Station; value.wifi.ssid = ssid;
        value.wifi.state = valid && !failed ? State::Connected : State::Failed;
        value.wifi.ipAddress = valid && !failed ? "192.168.1.42" : "";
        value.wifi.rssi = valid && !failed ? -48 : 0;
        value.wifi.lastError = !valid ? "Invalid SSID or password length/format" : failed ? "Simulated connection failure" : "";
    });
    return valid && !failed;
}
void HalDesktop::wifiDisconnect()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (lease && state.allowsCommands()) idle();
}
void HalDesktop::wifiForgetCredentials()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return;
    idle();
    state.update([](auto& value) { value.wifi.lastError = "Saved credentials cleared"; });
}
bool HalDesktop::wifiStartAccessPoint()
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease || !state.allowsCommands()) return false;
    state.update([](auto& value) {
        value.stationWanted = false; ++value.wifiGeneration;
        value.wifi.mode = Mode::AccessPoint; value.wifi.state = State::ApRunning;
        value.wifi.ssid = "M5Tab5-UserDemo-WiFi"; value.wifi.ipAddress = "192.168.4.1"; value.wifi.lastError.clear();
    });
    return true;
}
void HalDesktop::wifiStopAccessPoint() { wifiDisconnect(); }
hal::HalBase::OtaStatus HalDesktop::otaGetStatus()
{
    auto status = state.snapshot().ota;
    status.simulated = true;
    return status;
}
bool HalDesktop::otaStartFromUrl(const std::string& url)
{
    hal::NetworkCommandLease lease(commandBusy);
    if (!lease) return false;
    if (!hal::validOtaUrl(url)) {
        state.update([](auto& value) { if (!value.otaActive) { value.ota.lastError = "Use an HTTPS URL without credentials"; value.ota.state = OtaState::Failed; } });
        return false;
    }
    if (!state.reserveOta()) return false;
    state.update([](auto& value) { value.ota.simulated = true; });
    if (!otaRunner.start("Simulated firmware update", [url] { simulateOta(url); })) {
        state.update([](auto& value) { value.otaActive = false; value.ota.canCancel = false; value.ota.state = OtaState::Failed; value.ota.lastError = "Could not start OTA worker"; });
        return false;
    }
    return true;
}
void HalDesktop::otaCancel() { state.cancelOta(); }
void HalDesktop::otaRebootToApply()
{
    hal::NetworkCommandLease lease(commandBusy);
    const auto before = state.snapshot();
    if (lease && before.accepting && !before.otaActive && before.ota.rebootRequired)
        state.update([](auto& value) { value.ota = {}; value.ota.simulated = true; });
}

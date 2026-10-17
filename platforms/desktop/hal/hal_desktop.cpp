/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal_desktop.h"
#include <hal/profiles/tab5_gpio.h>
#include <SDL2/SDL.h>
#include <mooncake_log.h>
#include <random>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <map>

static const std::string _tag = "hal";
static std::mutex _ppsMutex;

static std::map<uint8_t, std::vector<uint8_t>>& pps_registers()
{
    static std::map<uint8_t, std::vector<uint8_t>> registers;
    static bool initialized = false;

    auto set_byte = [&](uint8_t reg, uint8_t value) { registers[reg] = {value}; };
    auto set_float = [&](uint8_t reg, float value) {
        std::vector<uint8_t> bytes(4);
        memcpy(bytes.data(), &value, sizeof(value));
        registers[reg] = bytes;
    };

    if (!initialized) {
        registers[0x00] = {0x41, 0x10};
        set_byte(0x01, 0x10);
        set_byte(0x04, 0x00);
        set_byte(0x05, 0x00);
        set_byte(0x07, 0x00);
        set_float(0x08, 0.0f);
        set_float(0x0C, 0.0f);
        set_float(0x10, 31.5f);
        set_float(0x14, 24.0f);
        set_float(0x18, 5.0f);
        set_float(0x1C, 1.0f);
        registers[0x52] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
        registers[0x5F] = {0x35};
        initialized = true;
    }

    bool enabled = registers[0x04][0] != 0;
    float vset = 0.0f;
    float iset = 0.0f;
    memcpy(&vset, registers[0x18].data(), sizeof(vset));
    memcpy(&iset, registers[0x1C].data(), sizeof(iset));

    float ripple = (rand() % 100) / 5000.0f;
    float vout = enabled ? (vset - ripple) : 0.0f;
    float iout = enabled ? std::min(iset, 0.65f + static_cast<float>(rand() % 20) / 100.0f) : 0.0f;
    set_float(0x08, vout);
    set_float(0x0C, iout);
    set_float(0x10, 31.0f + static_cast<float>(rand() % 40) / 10.0f);
    set_float(0x14, 24.0f + static_cast<float>(rand() % 12) / 100.0f);
    set_byte(0x05, enabled ? (iout >= iset ? 0x02 : 0x01) : 0x00);
    set_byte(0x07, static_cast<uint8_t>(registers[0x07][0] + 1));

    return registers;
}

void HalDesktop::init()
{
    mclog::tagInfo(_tag, "init");
    _wifi_status.extAntennaEnabled = _ext_antenna_enable;
    _wifi_networks = {
        {"Workshop", -42, true, 6},
        {"M5Lab", -58, true, 11},
        {"Guest", -76, false, 1},
    };
    const char* noNetworks = std::getenv("M5TAB5_DESKTOP_WIFI_NO_NETWORKS");
    if (noNetworks != nullptr && noNetworks[0] == '1') {
        _wifi_networks.clear();
    }
    lvgl_init();
}

/* -------------------------------------------------------------------------- */
/*                                   System                                   */
/* -------------------------------------------------------------------------- */
void HalDesktop::delay(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t HalDesktop::millis()
{
    return SDL_GetTicks();
}

int HalDesktop::getCpuTemp()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.99, 1.01);

    return dis(gen) * 45.0f;
}

/* -------------------------------------------------------------------------- */
/*                                   Display                                  */
/* -------------------------------------------------------------------------- */
void HalDesktop::setDisplayBrightness(uint8_t brightness)
{
    _current_lcd_brightness = std::clamp((int)brightness, 0, 100);
    mclog::tagInfo(_tag, "set display brightness: {}%", _current_lcd_brightness);
}

uint8_t HalDesktop::getDisplayBrightness()
{
    return _current_lcd_brightness;
}

/* -------------------------------------------------------------------------- */
/*                                Power monitor                               */
/* -------------------------------------------------------------------------- */
void HalDesktop::updatePowerMonitorData()
{
    std::lock_guard<std::mutex> lock(_powerMonitorIoMutex);
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.95, 1.0);

    PMData_t data;
    data.busVoltage = dis(gen) * 8.0f;
    data.shuntCurrent = -dis(gen) * 0.5f;
    data.busPower = data.busVoltage * -data.shuntCurrent;
    publishPowerMonitorData(data);
}

hal::PowerResult HalDesktop::performPowerAction(hal::PowerAction action, uint16_t rtcMinutes)
{
    std::lock_guard<std::mutex> actionLock(_powerActionMutex);
    delay(150);
    const char* fault = std::getenv("M5TAB5_DESKTOP_POWER_FAIL");
    if (fault && std::strcmp(fault, "1") == 0) return {false, false, true, "Simulated power action failure"};
    switch (action) {
        case hal::PowerAction::SleepTouch: {
            LvglLockGuard lock;
            if (_displayAsleep.load()) return {false, true, true, "Display already off"};
            _wakeBrightness = getDisplayBrightness();
            setDisplayBrightness(0);
            _displayAsleep.store(true);
            return {true, true, true, "Display off (simulated)"};
        }
        case hal::PowerAction::SleepShake:
            return {true, false, true, "Motion sleep simulated"};
        case hal::PowerAction::SleepRtc:
            if (rtcMinutes < 1 || rtcMinutes > 60) return {false, false, true, "Invalid wake interval"};
            return {true, false, true, "Timed sleep simulated: " + std::to_string(rtcMinutes) + " minutes"};
        case hal::PowerAction::PowerOff:
            return {true, false, true, "Power off simulated"};
    }
    return {false, false, true, "Unknown power action"};
}

bool HalDesktop::isDisplayAsleep() { return _displayAsleep.load(); }
bool HalDesktop::wakeDisplay()
{
    LvglLockGuard lock;
    if (_displayAsleep.load()) {
        setDisplayBrightness(_wakeBrightness);
        _displayAsleep.store(false);
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                     IMU                                    */
/* -------------------------------------------------------------------------- */
void HalDesktop::updateImuData()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-0.1, 0.1);

    imuData.accelX = dis(gen);
    imuData.accelY = dis(gen);
    imuData.accelZ = dis(gen);
}

/* -------------------------------------------------------------------------- */
/*                                    Power                                   */
/* -------------------------------------------------------------------------- */
void HalDesktop::setChargeQcEnable(bool enable)
{
    _charge_qc_enable = enable;
    mclog::tagInfo(_tag, "set charge qc enable: {}", _charge_qc_enable);
}

bool HalDesktop::getChargeQcEnable()
{
    return _charge_qc_enable;
}

void HalDesktop::setChargeEnable(bool enable)
{
    _charge_enable = enable;
    mclog::tagInfo(_tag, "set charge enable: {}", _charge_enable);
}

bool HalDesktop::getChargeEnable()
{
    return _charge_enable;
}

void HalDesktop::setUsb5vEnable(bool enable)
{
    _usba_5v_enable = enable;
    mclog::tagInfo(_tag, "set usb5v enable: {}", _usba_5v_enable);
}

bool HalDesktop::getUsb5vEnable()
{
    return _usba_5v_enable;
}

void HalDesktop::setExt5vEnable(bool enable)
{
    _ext_5v_enable = enable;
    mclog::tagInfo(_tag, "set ext5v enable: {}", _ext_5v_enable);
}

bool HalDesktop::getExt5vEnable()
{
    return _ext_5v_enable;
}

void HalDesktop::setExtAntennaEnable(bool enable)
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _ext_antenna_enable = enable;
    _wifi_status.extAntennaEnabled = enable;
    mclog::tagInfo(_tag, "set ext antenna enable: {}", _ext_antenna_enable);
}

bool HalDesktop::getExtAntennaEnable()
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    return _ext_antenna_enable;
}

hal::HalBase::WifiStatus HalDesktop::wifiGetStatus()
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _wifi_status.extAntennaEnabled = _ext_antenna_enable;
    return _wifi_status;
}

std::vector<hal::HalBase::WifiNetwork> HalDesktop::wifiScan()
{
    WifiConnectionState previous;
    {
        std::lock_guard<std::mutex> lock(_wifi_mutex);
        previous = _wifi_status.state == WifiConnectionState::Connected ||
                   _wifi_status.state == WifiConnectionState::ApRunning ? _wifi_status.state : WifiConnectionState::Idle;
        if (_wifi_status.mode == WifiMode::Off) {
            _wifi_status.mode = WifiMode::Station;
        }
        _wifi_status.state = WifiConnectionState::Scanning;
        _wifi_status.lastError.clear();
    }
    // Simulate a scan while keeping cached status readable by the UI thread.
    SDL_Delay(100);
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _wifi_status.state = previous;
    return _wifi_networks;
}

bool HalDesktop::wifiConnect(const std::string& ssid, const std::string& password)
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    (void)password;
    if (ssid.empty()) {
        _wifi_status.state = WifiConnectionState::Failed;
        _wifi_status.lastError = "SSID is required";
        return false;
    }

    const char* failConnect = std::getenv("M5TAB5_DESKTOP_WIFI_CONNECT_FAIL");
    if (failConnect != nullptr && failConnect[0] == '1') {
        _wifi_status.mode = WifiMode::Station;
        _wifi_status.state = WifiConnectionState::Failed;
        _wifi_status.ssid = ssid;
        _wifi_status.lastError = "Simulated connection failure";
        return false;
    }

    _wifi_status.mode = WifiMode::Station;
    _wifi_status.state = WifiConnectionState::Connected;
    _wifi_status.ssid = ssid;
    _wifi_status.ipAddress = "192.168.1.42";
    _wifi_status.rssi = -48;
    _wifi_status.lastError.clear();
    return true;
}

void HalDesktop::wifiDisconnect()
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _wifi_status.mode = WifiMode::Off;
    _wifi_status.state = WifiConnectionState::Idle;
    _wifi_status.ssid.clear();
    _wifi_status.ipAddress.clear();
    _wifi_status.rssi = 0;
}

void HalDesktop::wifiForgetCredentials()
{
    wifiDisconnect();
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _wifi_status.lastError = "Saved credentials cleared";
}

void HalDesktop::wifiSetExtAntennaEnabled(bool enable)
{
    setExtAntennaEnable(enable);
}

bool HalDesktop::wifiStartAccessPoint()
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    _wifi_status.mode = WifiMode::AccessPoint;
    _wifi_status.state = WifiConnectionState::ApRunning;
    _wifi_status.ssid = "M5Tab5-UserDemo-WiFi";
    _wifi_status.ipAddress = "192.168.4.1";
    _wifi_status.rssi = 0;
    _wifi_status.lastError.clear();
    return true;
}

void HalDesktop::wifiStopAccessPoint()
{
    wifiDisconnect();
}

hal::HalBase::OtaStatus HalDesktop::otaGetStatus()
{
    if (_ota_status.state == OtaState::Downloading || _ota_status.state == OtaState::Installing) {
        const uint32_t elapsed = millis() - _ota_started_ms;
        _ota_status.progress = std::min(100, static_cast<int>(elapsed / 40));
        _ota_status.totalBytes = 3 * 1024 * 1024;
        _ota_status.bytesReceived = (_ota_status.totalBytes * _ota_status.progress) / 100;
        if (_ota_status.progress >= 90 && _ota_status.state == OtaState::Downloading) {
            _ota_status.state = OtaState::Installing;
        }
        if (_ota_status.progress >= 100) {
            _ota_status.state = OtaState::Complete;
            _ota_status.rebootRequired = true;
        }
    }
    return _ota_status;
}

bool HalDesktop::otaStartFromUrl(const std::string& url)
{
    if (wifiGetStatus().state != WifiConnectionState::Connected) {
        _ota_status = {};
        _ota_status.state = OtaState::Failed;
        _ota_status.lastError = "Connect Wi-Fi before installing updates";
        return false;
    }
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        _ota_status = {};
        _ota_status.state = OtaState::Failed;
        _ota_status.lastError = "URL must start with http:// or https://";
        return false;
    }
    _ota_status = {};
    _ota_status.state = OtaState::Downloading;
    _ota_started_ms = millis();
    return true;
}

void HalDesktop::otaCancel()
{
    if (_ota_status.state == OtaState::Downloading || _ota_status.state == OtaState::Installing) {
        _ota_status = {};
        _ota_status.state = OtaState::Failed;
        _ota_status.lastError = "Update cancelled";
    }
}

void HalDesktop::otaRebootToApply()
{
    _ota_status = {};
}

/* -------------------------------------------------------------------------- */
/*                                   SD card                                  */
/* -------------------------------------------------------------------------- */
bool HalDesktop::isSdCardMounted()
{
    return true;
}

std::vector<hal::HalBase::FileEntry_t> HalDesktop::scanSdCard(const std::string& dirPath)
{
    std::filesystem::path path(dirPath);
    std::vector<hal::HalBase::FileEntry_t> file_entries;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        file_entries.push_back({entry.path().filename().string(), entry.is_directory()});
    }
    return file_entries;
}

/* -------------------------------------------------------------------------- */
/*                                  Interface                                 */
/* -------------------------------------------------------------------------- */
bool HalDesktop::usbCDetect()
{
    // return rand() % 2 == 0;
    return false;
}

bool HalDesktop::usbADetect()
{
    // return rand() % 2 == 0;
    return false;
}

bool HalDesktop::headPhoneDetect()
{
    // return rand() % 2 == 0;
    return false;
}

std::vector<uint8_t> HalDesktop::i2cScan(bool isInternal)
{
    // Random 6 addrs
    std::vector<uint8_t> addrs;
    for (int i = 0; i < 6; i++) {
        addrs.push_back(rand() % 128);
    }
    if (isInternal) {
        addrs.push_back(0x35);
    }
    return addrs;
}

bool HalDesktop::i2cProbe(hal::ModuleBus bus, uint8_t address)
{
    const char* absent = std::getenv("M5TAB5_DESKTOP_PPS_ABSENT");
    if (absent != nullptr && absent[0] == '1') {
        return false;
    }
    return bus == hal::ModuleBus::MBus && address == 0x35;
}

std::vector<hal::GpioPinInfo> HalDesktop::gpioGetPins() { return hal::tab5ExpansionPins(); }

namespace {
bool gpioFault(const char* action)
{
    const auto* fault = std::getenv("M5TAB5_DESKTOP_GPIO_FAIL");
    return fault != nullptr && std::strcmp(fault, action) == 0;
}
}

hal::GpioClaimResult HalDesktop::gpioInitOutput(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(_gpioMutex);
    if (pin >= _gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) || _gpioOutputs[pin].claimed) {
        return hal::GpioClaimResult::Rejected;
    }
    _gpioOutputs[pin] = {true, !gpioFault("init"), false};
    return _gpioOutputs[pin].levelKnown ? hal::GpioClaimResult::Claimed : hal::GpioClaimResult::Unknown;
}

bool HalDesktop::gpioSetLevel(uint8_t pin, bool level)
{
    std::lock_guard<std::mutex> lock(_gpioMutex);
    if (pin >= _gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) ||
        !_gpioOutputs[pin].claimed || !_gpioOutputs[pin].levelKnown) {
        return false;
    }
    auto& state = _gpioOutputs[pin];
    if (gpioFault("set")) {
        state.levelKnown = false;
        return false;
    }
    state.high = level;
    return true;
}

bool HalDesktop::gpioReset(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(_gpioMutex);
    if (pin >= _gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) || !_gpioOutputs[pin].claimed) {
        return false;
    }
    if (gpioFault("release")) {
        _gpioOutputs[pin].levelKnown = false;
        return false;
    }
    _gpioOutputs[pin] = {};
    return true;
}

hal::GpioOutputState HalDesktop::gpioGetOutputState(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(_gpioMutex);
    return pin < _gpioOutputs.size() ? _gpioOutputs[pin] : hal::GpioOutputState{};
}

bool HalDesktop::i2cReadRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, uint8_t* buffer, size_t length)
{
    std::lock_guard<std::mutex> lock(_ppsMutex);
    if (!i2cProbe(bus, address)) {
        return false;
    }

    auto& registers = pps_registers();
    auto it = registers.find(reg);
    if (it == registers.end() || it->second.size() < length) {
        return false;
    }

    memcpy(buffer, it->second.data(), length);
    return true;
}

bool HalDesktop::i2cWriteRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, const uint8_t* data, size_t length)
{
    std::lock_guard<std::mutex> lock(_ppsMutex);
    if (!i2cProbe(bus, address)) {
        return false;
    }

    auto& registers = pps_registers();
    registers[reg] = std::vector<uint8_t>(data, data + length);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                UART monitor                                */
/* -------------------------------------------------------------------------- */
void HalDesktop::uartMonitorSend(std::string msg, bool newLine)
{
    static bool is_test_thread_running = false;
    if (is_test_thread_running) {
        return;
    }

    is_test_thread_running = true;
    std::thread([&]() {
        for (int i = 0; i < 6; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> lock(GetHAL()->uartMonitorData.mutex);
            std::string send_msg = "[2025-04-24 14:32:38.111] [info] [panel-com] recv msg: 32\n";
            mclog::tagInfo(_tag, "send msg: {}", send_msg);
            for (auto c : send_msg) {
                GetHAL()->uartMonitorData.rxQueue.push(c);
            }
        }
        is_test_thread_running = false;
    }).detach();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal_desktop.h"
#include <hal/sd_scan.h>
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

bool HalDesktop::readCpuTemp(int& value)
{
    value = getCpuTemp();
    return true; // Synthetic desktop sensor, labelled by the PM snapshot.
}

int HalDesktop::getCpuTemp()
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
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
    data.valid = data.hasSample = data.simulated = true;
    data.sampledAtMs = millis();
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
    std::lock_guard<std::mutex> lock(_imuIoMutex);
    IMUData_t data;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-0.1, 0.1);

    data.accelX = dis(gen);
    data.accelY = dis(gen);
    data.accelZ = dis(gen);
    data.valid = data.hasSample = data.simulated = true;
    data.sampledAtMs = millis();
    publishImuData(data);
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

/* -------------------------------------------------------------------------- */
/*                                   SD card                                  */
/* -------------------------------------------------------------------------- */
bool HalDesktop::isSdCardMounted()
{
    return false; // Desktop directory scans do not mount hardware.
}

hal::HalBase::SdScanResult HalDesktop::scanSdCardResult(const std::string& dirPath)
{
    SdScanResult result{SdScanStatus::Success, {}, {}, true};
    std::error_code error;
    std::filesystem::directory_iterator it(dirPath, error), end;
    if (error) return {SdScanStatus::ReadFailed, {}, "Could not open desktop directory: " + error.message(), true};
    for (; it != end; it.increment(error)) {
        if (error) break;
        const bool isDir = it->is_directory(error);
        if (error) break;
        if (!hal::appendSdEntry(result, {it->path().filename().string(), isDir})) break;
    }
    if (error) return {SdScanStatus::ReadFailed, {}, "Could not read desktop directory: " + error.message(), true};
    return result;
}

/* -------------------------------------------------------------------------- */
/*                                  Interface                                 */
/* -------------------------------------------------------------------------- */
void HalDesktop::updateInterfaceData()
{
    std::lock_guard<std::mutex> ioLock(_interfaceIoMutex);
    const char* fail = std::getenv("M5TAB5_DESKTOP_INTERFACE_FAIL");
    if (fail && fail[0] == '1') {
        auto data = getInterfaceData();
        data.valid = false;
        data.simulated = true;
        data.error = "Simulated interface read failure";
        publishInterfaceData(data);
        return;
    }
    InterfaceData data;
    data.valid = true;
    data.hasSample = true;
    data.simulated = true;
    data.usbCKnown = true;
    data.headphoneKnown = true;
    data.usbAKnown = false;
    data.sampledAtMs = millis();
    publishInterfaceData(data);
}

bool HalDesktop::usbCDetect()
{
    const auto data = getInterfaceData();
    return data.valid && data.usbCKnown && data.usbC;
}

bool HalDesktop::usbADetect()
{
    const auto data = getInterfaceData();
    return data.valid && data.usbAKnown && data.usbA;
}

bool HalDesktop::headPhoneDetect()
{
    const auto data = getInterfaceData();
    return data.valid && data.headphoneKnown && data.headphone;
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
    if (reg == 0x04 && length == 1 && data[0] == 0 && std::getenv("M5TAB5_DESKTOP_PPS_OFF_FAIL")) return false;

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

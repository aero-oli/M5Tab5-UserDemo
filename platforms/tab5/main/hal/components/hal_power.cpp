/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <mooncake_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bsp/m5stack_tab5.h>
#include <esp_sleep.h>
#include <esp_check.h>
#include <esp_pm.h>
#include <modules/i2c_device.h>
#include <modules/rtc/rx8130_wake_timer.h>
#include <accel_gyro_bmi270.h>
#include <hal/power_recovery.h>

static const std::string _tag = "power";
HalEsp32* HalEsp32::_sleepOwner = nullptr;

void HalEsp32::updatePowerMonitorData()
{
    std::lock_guard<std::mutex> lock(_powerMonitorIoMutex);
    PMData_t data;
    if (!ina226.readMeasurements(data.busVoltage, data.busPower, data.shuntVoltage, data.shuntCurrent)) {
        publishPowerMonitorError("Power monitor read failed");
        return;
    }
    data.valid = data.hasSample = true;
    data.sampledAtMs = millis();
    publishPowerMonitorData(data);
}

void HalEsp32::setChargeQcEnable(bool enable)
{
    _charge_qc_enable = enable;
    mclog::tagInfo(_tag, "set charge qc enable: {}", _charge_qc_enable);
    bsp_set_charge_qc_en(_charge_qc_enable);
}

bool HalEsp32::getChargeQcEnable()
{
    return _charge_qc_enable;
}

void HalEsp32::setChargeEnable(bool enable)
{
    _charge_enable = enable;
    mclog::tagInfo(_tag, "set charge enable: {}", _charge_enable);
    bsp_set_charge_en(_charge_enable);
}

bool HalEsp32::getChargeEnable()
{
    return _charge_enable;
}

void HalEsp32::setUsb5vEnable(bool enable)
{
    _usba_5v_enable = enable;
    mclog::tagInfo(_tag, "set usb 5v enable: {}", _usba_5v_enable);
    bsp_set_usb_5v_en(_usba_5v_enable);
}

bool HalEsp32::getUsb5vEnable()
{
    return _usba_5v_enable;
}

void HalEsp32::setExt5vEnable(bool enable)
{
    _ext_5v_enable = enable;
    mclog::tagInfo(_tag, "set ext 5v enable: {}", _ext_5v_enable);
    bsp_set_ext_5v_en(_ext_5v_enable);
}

bool HalEsp32::getExt5vEnable()
{
    return _ext_5v_enable;
}

HalEsp32::~HalEsp32()
{
    lvglLock();
    if (_sleepInput && _touchReader && lv_indev_get_read_cb(_sleepInput) == readSleepTouch)
        lv_indev_set_read_cb(_sleepInput, _touchReader);
    if (_sleepOwner == this) _sleepOwner = nullptr;
    lvglUnlock();
}

bool HalEsp32::isDisplayAsleep() { return _displayAsleep.load(); }

bool HalEsp32::wakeDisplay()
{
    lvglLock();
    const bool ok = !_displayAsleep.load() || bsp_display_brightness_set(_wakeBrightness) == ESP_OK;
    if (ok && _displayAsleep.load()) {
        _current_lcd_brightness = _wakeBrightness;
        _displaySleepGate.wake();
        _displayAsleep.store(false);
    }
    lvglUnlock();
    return ok;
}

void HalEsp32::readSleepTouch(lv_indev_t* indev, lv_indev_data_t* data)
{
    auto* hal = _sleepOwner;
    if (!hal) { data->state = LV_INDEV_STATE_RELEASED; return; }
    hal->_touchReader(indev, data);
    const bool pressed = data->state == LV_INDEV_STATE_PRESSED;
    const auto filtered = hal->_displaySleepGate.filter(pressed);
    if (filtered.wake && !hal->wakeDisplay()) hal->_displaySleepGate.arm(true);
    if (filtered.consume) data->state = LV_INDEV_STATE_RELEASED;
}

hal::PowerResult HalEsp32::performPowerAction(hal::PowerAction action, uint16_t rtcMinutes)
{
    std::lock_guard<std::mutex> actionLock(_powerActionMutex);
    if (action == hal::PowerAction::SleepTouch) {
        LvglLockGuard lock;
        if (_displayAsleep.load()) return {false, true, false, "Display already off"};
        auto* input = bsp_display_get_input_dev();
        if (!input) return {false, false, false, "Touch wake unavailable"};
        if (!_touchReader) {
            _touchReader = lv_indev_get_read_cb(input);
            if (!_touchReader) return {false, false, false, "Touch reader unavailable"};
            _sleepInput = input;
            _sleepOwner = this;
            lv_indev_set_read_cb(input, readSleepTouch);
        }
        _wakeBrightness = getDisplayBrightness();
        if (bsp_display_brightness_set(0) != ESP_OK) return {false, false, false, "Display off failed"};
        _current_lcd_brightness = 0;
        _displaySleepGate.arm(lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED);
        _displayAsleep.store(true);
        return {true, true, false, "Display off"};
    }
    if (action != hal::PowerAction::PowerOff && action != hal::PowerAction::SleepShake &&
        action != hal::PowerAction::SleepRtc) return {false, false, false, "Unknown power action"};
    if (_displayAsleep.load()) return {false, true, false, "Wake display before shutdown"};
    if (action == hal::PowerAction::SleepRtc && (rtcMinutes < 1 || rtcMinutes > 60))
        return {false, false, false, "Wake interval must be 1-60 minutes"};

    std::lock_guard<std::mutex> rtcLock(_rtcIoMutex);
    modules::HalI2cDevice rtcDevice(hal::ModuleBus::MBus, 0x32);
    rtc::Rx8130WakeTimer timer(rtcDevice);
    auto failed = [&](const std::string& message) {
        auto result = hal::recoverPowerFailure(hal::PowerResult{false, false, false, message},
            [&] { return timer.cancel(); }, [&] { return accel_gyro_bmi270_restore_normal(); });
        resumeAudio(); // Reopen tone/playback admission only after preparation has unwound.
        return result;
    };
    if (!timer.cancel()) return failed(timer.error());
    if (accel_gyro_bmi270_init(bsp_i2c_get_handle()) != ESP_OK)
        return failed("IMU initialization failed");
    if (!accel_gyro_bmi270_disable_wake_irq()) return failed("Motion wake disable not verified");
    if (action == hal::PowerAction::SleepShake && !accel_gyro_bmi270_motion_irq())
        return failed("Motion wake configuration failed");

    // Cancel every codec user and close admission until poweroff or recovery.
    if (!stopAudioAndWait(2000)) return failed("Audio did not stop; shutdown cancelled");
    if (action == hal::PowerAction::SleepRtc && !timer.arm(rtcMinutes * 60))
        return failed(timer.error());
    uint8_t brightness = 0;
    bool dimmed = false;
    {
        LvglLockGuard lock;
        brightness = getDisplayBrightness();
        dimmed = bsp_display_brightness_set(0) == ESP_OK;
        if (dimmed) _current_lcd_brightness = 0;
    }
    if (!dimmed) return failed("Display off failed; shutdown cancelled");
    bsp_generate_poweroff_signal();
    delay(1500);
    // A functioning board power switch never returns here; reporting success would hide a failed shutdown.
    {
        LvglLockGuard lock;
        setDisplayBrightness(brightness);
    }
    return failed("Power-off signal sent but device is still running");
}

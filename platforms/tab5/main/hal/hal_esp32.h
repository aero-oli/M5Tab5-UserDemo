/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <hal/hal.h>
#include <ina226.hpp>
#include <lvgl.h>
#include <atomic>
#include <hal/display_sleep_gate.h>

class HalEsp32 : public hal::HalBase {
public:
    ~HalEsp32() override;
    std::string type() override
    {
        return "Tab5";
    }

    void init() override;

    void delay(uint32_t ms) override;
    uint32_t millis() override;
    int getCpuTemp() override;
    bool readCpuTemp(int& value) override;

    INA226 ina226;
    lv_disp_t* lvDisp      = nullptr;
    lv_indev_t* lvKeyboard = nullptr;

    void setDisplayBrightness(uint8_t brightness) override;
    uint8_t getDisplayBrightness() override;

    void lvglLock() override;
    void lvglUnlock() override;

    void updatePowerMonitorData() override;
    void updateImuData() override;
    void clearImuIrq() override;

    void clearRtcIrq() override;
    void updateRtcData() override;
    bool setRtcTimeChecked(const tm& time, bool initialize = false) override;

    void setChargeQcEnable(bool enable) override;
    bool getChargeQcEnable() override;
    void setChargeEnable(bool enable) override;
    bool getChargeEnable() override;
    void setUsb5vEnable(bool enable) override;
    bool getUsb5vEnable() override;
    void setExt5vEnable(bool enable) override;
    bool getExt5vEnable() override;
    hal::PowerResult performPowerAction(hal::PowerAction action, uint16_t rtcMinutes) override;
    bool isDisplayAsleep() override;
    bool wakeDisplay() override;

    bool startCameraCapture(lv_obj_t* imgCanvas) override;
    void stopCameraCapture() override;
    bool shutdownCamera(uint32_t timeoutMs) override;
    CameraStatus cameraGetStatus() override;
    bool isCameraCapturing() override;

    AudioStatus audioGetStatus() override;
    void audioCancel() override;
    bool stopAudioAndWait(uint32_t timeoutMs) override;
    void resumeAudio() override;
    void setSpeakerVolume(uint8_t volume) override;
    uint8_t getSpeakerVolume() override;
    void audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain = 80.0f) override;
    void audioPlay(std::vector<int16_t>& data, bool async = true) override;
    void startDualMicRecordTest() override;
    MicTestState_t getDualMicRecordTestState() override;
    void startHeadphoneMicRecordTest() override;
    MicTestState_t getHeadphoneMicRecordTestState() override;
    void startPlayMusicTest() override;
    MusicPlayState_t getMusicPlayTestState() override;
    void stopPlayMusicTest() override;
    void playStartupSfx() override;
    void playShutdownSfx() override;

    void setExtAntennaEnable(bool enable) override;
    bool getExtAntennaEnable() override;
    void startWifiAp() override;
    bool wifiRestoreSavedConnection() override;
    bool stopNetworkAndWait(uint32_t timeoutMs) override;
    bool stopUartAndWait(uint32_t timeoutMs) override;
    void resumeNetwork() override;
    WifiStatus wifiGetStatus() override;
    std::vector<WifiNetwork> wifiScan() override;
    bool wifiConnect(const std::string& ssid, const std::string& password) override;
    void wifiDisconnect() override;
    void wifiForgetCredentials() override;
    void wifiSetExtAntennaEnabled(bool enable) override;
    bool wifiStartAccessPoint() override;
    void wifiStopAccessPoint() override;
    OtaStatus otaGetStatus() override;
    bool otaStartFromUrl(const std::string& url) override;
    void otaCancel() override;
    void otaRebootToApply() override;

    bool isSdCardMounted() override;
    SdScanResult scanSdCardResult(const std::string& dirPath) override;

    void updateInterfaceData() override;
    bool usbCDetect() override;
    bool usbADetect() override;
    bool headPhoneDetect() override;
    std::vector<uint8_t> i2cScan(bool isInternal) override;
    bool i2cProbe(hal::ModuleBus bus, uint8_t address) override;
    bool i2cReadRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) override;
    bool i2cWriteRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, const uint8_t* data, size_t length) override;
    void initPortAI2c() override;
    void deinitPortAI2c() override;
    std::vector<hal::GpioPinInfo> gpioGetPins() override;
    hal::GpioClaimResult gpioInitOutput(uint8_t pin) override;
    bool gpioSetLevel(uint8_t pin, bool level) override;
    bool gpioReset(uint8_t pin) override;
    hal::GpioOutputState gpioGetOutputState(uint8_t pin) override;

private:
    std::atomic<bool> _sdMounted {false};
    static void readSleepTouch(lv_indev_t* indev, lv_indev_data_t* data);
    static HalEsp32* _sleepOwner;
    std::mutex _powerActionMutex;
    std::atomic<bool> _displayAsleep{false};
    hal::DisplaySleepGate _displaySleepGate;
    lv_indev_t* _sleepInput = nullptr;
    lv_indev_read_cb_t _touchReader = nullptr;
    uint8_t _wakeBrightness = 100;
    void set_gpio_output_capability();
    void hid_init();
    void rs485_init();
    bool wifi_init();
    void wifi_set_status(WifiConnectionState state, const std::string& error = "");
    void imu_init();
    void update_system_time();

    uint8_t _current_lcd_brightness = 100;
    bool _charge_qc_enable          = false;
    bool _charge_enable             = true;
    bool _ext_5v_enable             = true;
    bool _usba_5v_enable            = true;
    bool _ext_antenna_enable        = false;
    bool _sd_card_mounted           = false;
    WifiStatus _wifi_status;
    OtaStatus _ota_status;
};

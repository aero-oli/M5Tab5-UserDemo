/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <hal/hal.h>
#include <mutex>
#include <array>
#include <atomic>

class HalDesktop : public hal::HalBase {
public:
    ~HalDesktop() override;
    std::string type() override
    {
        return "Desktop";
    }

    void init() override;

    void delay(uint32_t ms) override;
    uint32_t millis() override;
    int getCpuTemp() override;
    bool readCpuTemp(int& value) override;

    void setDisplayBrightness(uint8_t brightness) override;
    uint8_t getDisplayBrightness() override;

    void lvglLock() override;
    void lvglUnlock() override;

    bool startCameraCapture(lv_obj_t* canvas) override;
    void stopCameraCapture() override;
    CameraStatus cameraGetStatus() override;

    AudioStatus audioGetStatus() override;
    void audioCancel() override;
    bool stopAudioAndWait(uint32_t timeoutMs) override;
    void resumeAudio() override;
    void setSpeakerVolume(uint8_t volume) override;
    uint8_t getSpeakerVolume() override;
    void audioPlay(std::vector<int16_t>& data, bool async = true) override;
    void audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain = 80.0f) override;
    void startDualMicRecordTest() override;
    MicTestState_t getDualMicRecordTestState() override;
    void startHeadphoneMicRecordTest() override;
    MicTestState_t getHeadphoneMicRecordTestState() override;
    void startPlayMusicTest() override;
    MusicPlayState_t getMusicPlayTestState() override;
    void stopPlayMusicTest() override;
    void playStartupSfx() override;
    void playShutdownSfx() override;

    void updateRtcData() override;
    bool setRtcTimeChecked(const tm& time, bool initialize = false) override;
    void updatePowerMonitorData() override;
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

    void updateImuData() override;

    void setExtAntennaEnable(bool enable) override;
    bool getExtAntennaEnable() override;
    bool wifiRestoreSavedConnection() override;
    bool stopNetworkAndWait(uint32_t timeoutMs) override;
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
    std::vector<hal::GpioPinInfo> gpioGetPins() override;
    hal::GpioClaimResult gpioInitOutput(uint8_t pin) override;
    bool gpioSetLevel(uint8_t pin, bool level) override;
    bool gpioReset(uint8_t pin) override;
    hal::GpioOutputState gpioGetOutputState(uint8_t pin) override;
    std::vector<uint8_t> i2cScan(bool isInternal) override;
    bool i2cProbe(hal::ModuleBus bus, uint8_t address) override;
    bool i2cReadRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) override;
    bool i2cWriteRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, const uint8_t* data, size_t length) override;

    bool uartMonitorSend(std::string msg, bool newLine = true) override;

private:
    std::mutex _powerActionMutex;
    std::atomic<bool> _displayAsleep{false};
    uint8_t _wakeBrightness = 100;
    std::mutex _gpioMutex;
    std::array<hal::GpioOutputState, 55> _gpioOutputs {};
    uint8_t _current_lcd_brightness = 100;
    uint8_t _current_speaker_volume = 20;
    bool _charge_qc_enable          = false;
    bool _charge_enable             = true;
    bool _ext_5v_enable             = true;
    bool _usba_5v_enable            = true;
    std::mutex _cameraMutex;
    CameraStatus _cameraStatus{CameraState::Stopped};
    lv_obj_t* _cameraCanvas = nullptr;
    lv_timer_t* _cameraTimer = nullptr;
    std::vector<uint16_t> _cameraBuffers[2];
    uint32_t _cameraStartedMs = 0;
    int _cameraNextBuffer = 0;

    tm _rtcSimulatedTime{};
    uint32_t _rtcSimulatedAtMs = 0;
    bool _rtcSimulatedSet = false;
    void updateCameraFrame();
    void detachCamera();

    void lvgl_init();
};

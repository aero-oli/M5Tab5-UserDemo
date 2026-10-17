/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "gpio_diagnostics.h"
#include "power_control.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <lvgl.h>
#include <mutex>
#include <vector>

/**
 * @brief Hardware abstraction layer
 *
 */
namespace hal {

enum class ModuleBus {
    MBus,
    PortA,
};

/**
 * @brief
 *
 */
class HalBase {
public:
    virtual ~HalBase() = default;

    /**
     * @brief
     *
     * @return std::string
     */
    virtual std::string type()
    {
        return "Base";
    }

    /**
     * @brief
     *
     */
    virtual void init()
    {
    }

    /* --------------------------------- System --------------------------------- */
    virtual void delay(uint32_t ms)
    {
    }
    virtual uint32_t millis()
    {
        return 0;
    }
    virtual int getCpuTemp()
    {
        return 0.0f;
    }

    /* --------------------------------- Display -------------------------------- */
    virtual int getDisplayWidth()
    {
        return 1280;
    }
    virtual int getDisplayHeight()
    {
        return 720;
    }
    virtual void setDisplayBrightness(uint8_t brightness)
    {
    }
    virtual uint8_t getDisplayBrightness()
    {
        return 0;
    }

    /* ---------------------------------- Lvgl ---------------------------------- */
    lv_indev_t* lvTouchpad = nullptr;
    virtual void lvglLock()
    {
    }
    virtual void lvglUnlock()
    {
    }

    /* ---------------------------------- Power --------------------------------- */
    struct PMData_t {
        float busVoltage   = 0.0f;
        float busPower     = 0.0f;
        float shuntVoltage = 0.0f;
        float shuntCurrent = 0.0f;
    };
    PMData_t getPowerMonitorData()
    {
        std::lock_guard<std::mutex> lock(_powerMonitorMutex);
        return _powerMonitorData;
    }
    virtual void updatePowerMonitorData()
    {
    }
    virtual void setChargeQcEnable(bool enable)
    {
    }
    virtual bool getChargeQcEnable()
    {
        return false;
    }
    virtual void setChargeEnable(bool enable)
    {
    }
    virtual bool getChargeEnable()
    {
        return false;
    }
    virtual void setUsb5vEnable(bool enable)
    {
    }
    virtual bool getUsb5vEnable()
    {
        return false;
    }
    virtual void setExt5vEnable(bool enable)
    {
    }
    virtual bool getExt5vEnable()
    {
        return false;
    }
    // Bounded platform work; called only by the power controller's background worker.
    virtual PowerResult performPowerAction(PowerAction action, uint16_t rtcMinutes)
    {
        return {false, false, false, "Power control unavailable"};
    }
    virtual bool isDisplayAsleep() { return false; }
    virtual bool wakeDisplay() { return false; }

protected:
    std::mutex _powerMonitorMutex;
    std::mutex _powerMonitorIoMutex;
    PMData_t _powerMonitorData;
    void publishPowerMonitorData(const PMData_t& data)
    {
        std::lock_guard<std::mutex> lock(_powerMonitorMutex);
        _powerMonitorData = data;
    }
public:

    /* ----------------------------------- IMU ---------------------------------- */
    struct IMUData_t {
        float accelX = 0.0f;
        float accelY = 0.0f;
        float accelZ = 0.0f;
        float gyroX  = 0.0f;
        float gyroY  = 0.0f;
        float gyroZ  = 0.0f;
    };
    IMUData_t getImuData()
    {
        std::lock_guard<std::mutex> lock(_imuMutex);
        return _imuData;
    }
    virtual void updateImuData()
    {
    }
    virtual void clearImuIrq()
    {
    }

protected:
    std::mutex _imuMutex;
    std::mutex _imuIoMutex;
    IMUData_t _imuData;
    void publishImuData(const IMUData_t& data)
    {
        std::lock_guard<std::mutex> lock(_imuMutex);
        _imuData = data;
    }
public:

    /* ----------------------------------- RTC ---------------------------------- */
    virtual void getRtcTime(tm* time)
    {
    }
    virtual void setRtcTime(tm time)
    {
    }
    virtual void clearRtcIrq()
    {
    }

    /* --------------------------------- Camera --------------------------------- */
    enum class CameraState {
        Unavailable,
        Stopped,
        Starting,
        Running,
        Stopping,
        Failed,
    };
    struct CameraStatus {
        CameraState state = CameraState::Unavailable;
        std::string lastError;
        uint32_t frames = 0;
        int width = 0;
        int height = 0;
        bool simulated = false;
    };
    virtual bool startCameraCapture(lv_obj_t* imgCanvas)
    {
        return false;
    }
    // Must detach the LVGL target before returning; hardware cleanup may finish asynchronously.
    virtual void stopCameraCapture()
    {
    }
    // Process teardown only; call outside the display lock. A false result
    // forbids HAL destruction because the capture worker may still use it.
    virtual bool shutdownCamera(uint32_t timeoutMs)
    {
        stopCameraCapture();
        return true; // Base/synchronous platforms own no independent camera task.
    }
    virtual CameraStatus cameraGetStatus()
    {
        return {};
    }
    virtual bool isCameraCapturing()
    {
        return cameraGetStatus().state == CameraState::Running;
    }

    /* ---------------------------------- USB-A --------------------------------- */
    struct HidMouseData_t {
        std::mutex mutex;
        int x         = 0;
        int y         = 0;
        bool btnLeft  = false;
        bool btnRight = false;
    };
    HidMouseData_t hidMouseData;

    /* ---------------------------------- Audio --------------------------------- */
    virtual void setSpeakerVolume(uint8_t volume)
    {
    }
    virtual uint8_t getSpeakerVolume()
    {
        return 0;
    }
    enum class AudioOperation { None, Tone, Record, DualMic, HeadphoneMic, Music, Startup, Shutdown };
    enum class AudioPhase { Idle, Starting, Recording, Playing, Cancelling, Failed };
    struct AudioStatus {
        AudioOperation operation = AudioOperation::None;
        AudioPhase phase = AudioPhase::Idle;
        bool busy = false;
        std::string lastError;
    };
    virtual AudioStatus audioGetStatus() { return {}; }
    // Nonblocking cancellation. Draining also closes admission until resumeAudio().
    virtual void audioCancel() {}
    virtual bool stopAudioAndWait(uint32_t timeoutMs) { return true; }
    virtual void resumeAudio() {}

    // [MIC-L, AEC, MIC-R, MIC-HP]
    virtual void audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain = 80.0f)
    {
    }
    virtual void audioPlay(std::vector<int16_t>& data, bool async = true)
    {
    }

    // Mic record test
    enum MicTestState_t {
        MIC_TEST_IDLE,
        MIC_TEST_RECORDING,
        MIC_TEST_PLAYING,
    };
    virtual void startDualMicRecordTest()
    {
    }
    virtual MicTestState_t getDualMicRecordTestState()
    {
        return MIC_TEST_IDLE;
    }
    virtual void startHeadphoneMicRecordTest()
    {
    }
    virtual MicTestState_t getHeadphoneMicRecordTestState()
    {
        return MIC_TEST_IDLE;
    }

    // Play music test
    enum MusicPlayState_t {
        MUSIC_PLAY_IDLE,
        MUSIC_PLAY_PLAYING,
    };
    virtual void startPlayMusicTest()
    {
    }
    virtual MusicPlayState_t getMusicPlayTestState()
    {
        return MUSIC_PLAY_IDLE;
    }
    virtual void stopPlayMusicTest()
    {
    }

    // Sfx
    virtual void playStartupSfx()
    {
    }
    virtual void playShutdownSfx()
    {
    }

    /* --------------------------------- Network -------------------------------- */
    virtual void setExtAntennaEnable(bool enable)
    {
    }
    virtual bool getExtAntennaEnable()
    {
        return false;
    }
    virtual void startWifiAp()
    {
    }
    enum class WifiMode {
        Off,
        Station,
        AccessPoint,
        StationAndAp,
    };
    enum class WifiConnectionState {
        Idle,
        Scanning,
        Connecting,
        Connected,
        Failed,
        ApRunning,
    };
    struct WifiNetwork {
        std::string ssid;
        int rssi = 0;
        bool secure = true;
        uint8_t channel = 0;
    };
    struct WifiStatus {
        WifiMode mode = WifiMode::Off;
        WifiConnectionState state = WifiConnectionState::Idle;
        std::string ssid;
        std::string ipAddress;
        int rssi = 0;
        bool extAntennaEnabled = false;
        std::string lastError;
    };
    enum class OtaState {
        Idle,
        Checking,
        Downloading,
        Installing,
        Complete,
        Failed,
    };
    struct OtaStatus {
        OtaState state = OtaState::Idle;
        int progress = 0;
        size_t bytesReceived = 0;
        size_t totalBytes = 0;
        std::string lastError;
        bool rebootRequired = false;
    };
    virtual WifiStatus wifiGetStatus()
    {
        return {};
    }
    virtual std::vector<WifiNetwork> wifiScan()
    {
        return {};
    }
    virtual bool wifiConnect(const std::string& ssid, const std::string& password)
    {
        (void)ssid;
        (void)password;
        return false;
    }
    virtual void wifiDisconnect()
    {
    }
    virtual void wifiForgetCredentials()
    {
    }
    virtual void wifiSetExtAntennaEnabled(bool enable)
    {
        setExtAntennaEnable(enable);
    }
    virtual bool wifiStartAccessPoint()
    {
        startWifiAp();
        return true;
    }
    virtual void wifiStopAccessPoint()
    {
    }
    virtual OtaStatus otaGetStatus()
    {
        return {};
    }
    virtual bool otaStartFromUrl(const std::string& url)
    {
        (void)url;
        return false;
    }
    virtual void otaCancel()
    {
    }
    virtual void otaRebootToApply()
    {
    }

    /* --------------------------------- SD Card -------------------------------- */
    struct FileEntry_t {
        std::string name;
        bool isDir;
    };
    virtual bool isSdCardMounted()
    {
        return false;
    }
    virtual std::vector<FileEntry_t> scanSdCard(const std::string& dirPath)
    {
        return {};
    }

    /* -------------------------------- Interface ------------------------------- */
    virtual bool usbCDetect()
    {
        return false;
    }
    virtual bool usbADetect()
    {
        return false;
    }
    virtual bool headPhoneDetect()
    {
        return false;
    }
    virtual std::vector<uint8_t> i2cScan(bool isInternal)
    {
        return {};
    }
    virtual bool i2cProbe(ModuleBus bus, uint8_t address)
    {
        return false;
    }
    virtual bool i2cReadRegister(ModuleBus bus, uint8_t address, uint8_t reg, uint8_t* buffer, size_t length)
    {
        return false;
    }
    virtual bool i2cWriteRegister(ModuleBus bus, uint8_t address, uint8_t reg, const uint8_t* data, size_t length)
    {
        return false;
    }
    virtual void initPortAI2c()
    {
    }
    virtual void deinitPortAI2c()
    {
    }

    virtual std::vector<GpioPinInfo> gpioGetPins()
    {
        return {};
    }
    virtual GpioClaimResult gpioInitOutput(uint8_t pin)
    {
        return GpioClaimResult::Rejected;
    }
    virtual bool gpioSetLevel(uint8_t pin, bool level)
    {
        return false;
    }
    virtual bool gpioReset(uint8_t pin)
    {
        return false;
    }
    virtual GpioOutputState gpioGetOutputState(uint8_t pin)
    {
        return {};
    }

    /* ------------------------------ UART monitor ------------------------------ */
    struct UartMonitorData_t {
        std::mutex mutex;
        std::queue<uint8_t> rxQueue;
        std::queue<uint8_t> txQueue;
    };
    UartMonitorData_t uartMonitorData;
    virtual void uartMonitorSend(std::string msg, bool newLine = true)
    {
        std::lock_guard<std::mutex> lock(uartMonitorData.mutex);
        for (auto c : msg) {
            uartMonitorData.txQueue.push(c);
        }
        if (newLine) {
            uartMonitorData.txQueue.push('\n');
        }
    }
};

/**
 * @brief Get the HAL instance
 *
 * @return HalBase&
 */
HalBase* Get();

/**
 * @brief Inject the HAL, which will call init() to initialize the HAL
 *
 * @param hal
 */
void Inject(std::unique_ptr<HalBase> hal);

/**
 * @brief Destroy the HAL instance
 *
 */
void Destroy();

/**
 * @brief Check if the HAL instance exists
 *
 * @return true
 * @return false
 */
bool Check();

}  // namespace hal

/**
 * @brief Get the HAL instance
 *
 * @return hal::HalBase&
 */
inline hal::HalBase* GetHAL()
{
    return hal::Get();
}

/**
 * @brief
 *
 */
class LvglLockGuard {
public:
    LvglLockGuard()
    {
        GetHAL()->lvglLock();
    }
    ~LvglLockGuard()
    {
        GetHAL()->lvglUnlock();
    }
};

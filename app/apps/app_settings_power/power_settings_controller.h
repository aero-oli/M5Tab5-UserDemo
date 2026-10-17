#pragma once
#include <functional>
#include <memory>
#include <string>
#include <hal/hal.h>

namespace app_settings_power {
using PowerAction = hal::PowerAction;
enum class PowerState { Ready, Confirming, Preparing, DisplayAsleep, Complete, Failed };
struct PowerSettingsSample { float busVoltage = 0; float busPower = 0; };
struct PowerSettingsState {
    PowerState state = PowerState::Ready;
    PowerAction action = PowerAction::SleepTouch;
    uint16_t rtcMinutes = 5;
    PowerSettingsSample sample;
    bool hasSample = false;
    bool simulated = false;
    bool displayAsleep = false;
    std::string message = "Ready";
};

class PowerSettingsController {
public:
    using SampleFn = std::function<hal::HalBase::PMData_t()>;
    using PerformFn = std::function<hal::PowerResult(PowerAction, uint16_t)>;
    using AsleepFn = std::function<bool()>;
    using WakeFn = std::function<bool()>;
    PowerSettingsController();
    PowerSettingsController(SampleFn sample, PerformFn perform, AsleepFn asleep, WakeFn wake);
    ~PowerSettingsController();
    PowerSettingsController(const PowerSettingsController&) = delete;
    PowerSettingsController& operator=(const PowerSettingsController&) = delete;
    void update(uint32_t nowMs);
    PowerSettingsState state() const;
    bool request(PowerAction action, uint16_t rtcMinutes = 5);
    bool confirm();
    void cancel();
    bool wakeDisplay();
    static std::string actionLabel(PowerAction action);
    static std::string confirmation(const PowerSettingsState& state);
    static std::string formatBatteryInput(const PowerSettingsSample& sample);
    static void shutdown();
private:
    struct Worker;
    std::shared_ptr<Worker> _worker;
    uint32_t _lastPoll = 0;
    bool _polled = false;
    static std::shared_ptr<Worker>& defaultWorker();
    static void kick(const std::shared_ptr<Worker>& worker);
};
}

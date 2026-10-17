#include "power_settings_controller.h"
#include <apps/runtime/async_job_runner.h>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <optional>

using namespace app_settings_power;

struct PowerSettingsController::Worker {
    Worker(SampleFn sample, PerformFn perform, AsleepFn asleep, WakeFn wake)
        : sample(std::move(sample)), perform(std::move(perform)), asleep(std::move(asleep)), wake(std::move(wake)) {}
    SampleFn sample;
    PerformFn perform;
    AsleepFn asleep;
    WakeFn wake;
    mutable std::mutex mutex;
    PowerSettingsState state;
    std::optional<PowerSettingsState> command;
    bool poll = false, wakeRequested = false, working = false;
    void drain()
    {
        for (;;) {
            std::optional<PowerSettingsState> next;
            bool read = false, restore = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (command) { next = command; command.reset(); }
                else if (wakeRequested) { restore = true; wakeRequested = false; }
                else if (poll) { read = true; poll = false; }
                else { working = false; return; }
            }
            if (next) {
                const auto result = perform(next->action, next->rtcMinutes);
                std::lock_guard<std::mutex> lock(mutex);
                state.state = !result.success ? PowerState::Failed :
                              result.displayAsleep ? PowerState::DisplayAsleep : PowerState::Complete;
                state.simulated = result.simulated;
                state.displayAsleep = result.displayAsleep;
                state.message = result.message.empty() ? (result.success ? "Completed" : "Power action failed") : result.message;
            } else if (restore) {
                const bool ok = wake();
                const bool sleeping = asleep();
                std::lock_guard<std::mutex> lock(mutex);
                state.state = ok ? PowerState::Ready : PowerState::Failed;
                state.message = ok ? "Display awake" : "Display wake failed";
                state.displayAsleep = sleeping;
            } else if (read) {
                const auto data = sample();
                const bool sleeping = asleep();
                std::lock_guard<std::mutex> lock(mutex);
                const bool hasLastGood = data.hasSample && std::isfinite(data.busVoltage) && std::isfinite(data.busPower);
                state.sampleValid = data.valid && hasLastGood;
                state.sampleError = data.error;
                state.sampleSimulated = data.simulated;
                if (hasLastGood) {
                    state.hasSample = true;
                    state.sampledAtMs = data.sampledAtMs;
                    state.sample = {data.busVoltage, data.busPower};
                }
                if (state.displayAsleep && !sleeping &&
                    (state.state == PowerState::DisplayAsleep || state.state == PowerState::Failed)) {
                    state.state = PowerState::Ready;
                    state.message = "Display awake";
                }
                state.displayAsleep = sleeping;
            }
        }
    }
};

std::shared_ptr<PowerSettingsController::Worker>& PowerSettingsController::defaultWorker()
{
    static std::shared_ptr<Worker> worker;
    return worker;
}

PowerSettingsController::PowerSettingsController()
{
    auto& shared = defaultWorker();
    if (!shared) {
        shared = std::make_shared<Worker>(
            [] { GetHAL()->updatePowerMonitorData(); return GetHAL()->getPowerMonitorData(); },
            [](PowerAction action, uint16_t minutes) { return GetHAL()->performPowerAction(action, minutes); },
            [] { return GetHAL()->isDisplayAsleep(); }, [] { return GetHAL()->wakeDisplay(); });
    }
    _worker = shared;
}

PowerSettingsController::PowerSettingsController(SampleFn sample, PerformFn perform, AsleepFn asleep, WakeFn wake)
    : _worker(std::make_shared<Worker>(std::move(sample), std::move(perform), std::move(asleep), std::move(wake))) {}
PowerSettingsController::~PowerSettingsController() { cancel(); }

void PowerSettingsController::kick(const std::shared_ptr<Worker>& worker)
{
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (worker->working || (!worker->command && !worker->poll && !worker->wakeRequested)) return;
        worker->working = true;
    }
    auto runner = std::make_shared<app_runtime::AsyncJobRunner>();
    if (!runner->start("Power control", [worker, runner] { worker->drain(); })) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->working = false;
        worker->command.reset();
        worker->poll = worker->wakeRequested = false;
        worker->state.state = PowerState::Failed;
        worker->state.message = "Power worker unavailable";
    }
}

void PowerSettingsController::update(uint32_t nowMs)
{
    if (!_polled || nowMs - _lastPoll >= 500) {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        _worker->poll = true;
        _polled = true;
        _lastPoll = nowMs;
    }
    kick(_worker);
}

PowerSettingsState PowerSettingsController::state() const
{
    std::lock_guard<std::mutex> lock(_worker->mutex);
    return _worker->state;
}

bool PowerSettingsController::request(PowerAction action, uint16_t minutes)
{
    if (action == PowerAction::SleepRtc && (minutes < 1 || minutes > 60)) return false;
    switch (action) {
        case PowerAction::SleepTouch: case PowerAction::SleepShake:
        case PowerAction::SleepRtc: case PowerAction::PowerOff: break;
        default: return false;
    }
    std::lock_guard<std::mutex> lock(_worker->mutex);
    if (_worker->state.state == PowerState::Confirming || _worker->state.state == PowerState::Preparing ||
        _worker->state.displayAsleep) return false;
    _worker->state.state = PowerState::Confirming;
    _worker->state.action = action;
    _worker->state.rtcMinutes = minutes;
    _worker->state.message = "Confirmation required";
    return true;
}

bool PowerSettingsController::confirm()
{
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        if (_worker->state.state != PowerState::Confirming) return false;
        _worker->state.state = PowerState::Preparing;
        _worker->state.message = "Preparing " + actionLabel(_worker->state.action);
        _worker->command = _worker->state;
    }
    kick(_worker);
    return true;
}

void PowerSettingsController::cancel()
{
    std::lock_guard<std::mutex> lock(_worker->mutex);
    if (_worker->state.state == PowerState::Confirming) {
        _worker->state.state = PowerState::Ready;
        _worker->state.message = "Cancelled";
    }
}

bool PowerSettingsController::wakeDisplay()
{
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        if (!_worker->state.displayAsleep || _worker->state.state == PowerState::Preparing) return false;
        _worker->wakeRequested = true;
        _worker->state.state = PowerState::Preparing;
        _worker->state.message = "Waking display";
    }
    kick(_worker);
    return true;
}

void PowerSettingsController::shutdown()
{
    auto worker = defaultWorker();
    if (!worker) return;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->poll = false;
    }
    kick(worker);
    // Active jobs own the worker. The process-wide bounded drain waits before HAL destruction.
    defaultWorker().reset();
}

std::string PowerSettingsController::actionLabel(PowerAction action)
{
    switch (action) {
        case PowerAction::SleepTouch: return "Screen Off";
        case PowerAction::SleepShake: return "Motion Sleep";
        case PowerAction::SleepRtc: return "Timed Sleep";
        case PowerAction::PowerOff: return "Power Off";
    }
    return "Power action";
}

std::string PowerSettingsController::confirmation(const PowerSettingsState& state)
{
    switch (state.action) {
        case PowerAction::SleepTouch: return "The display will turn off. Apps and Wi-Fi remain active.";
        case PowerAction::SleepShake: return "The device will shut down and wake on motion. Unsaved work will be lost.";
        case PowerAction::SleepRtc: return "The device will shut down and wake in " + std::to_string(state.rtcMinutes) +
                                          " minutes. Unsaved work will be lost.";
        case PowerAction::PowerOff: return "The device will shut down. Unsaved work will be lost.";
    }
    return "";
}

std::string PowerSettingsController::formatBatteryInput(const PowerSettingsSample& sample)
{
    char text[48];
    std::snprintf(text, sizeof(text), "%.2f V / %.2f W", sample.busVoltage, sample.busPower);
    return text;
}

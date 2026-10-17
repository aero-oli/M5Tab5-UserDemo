/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "pps_controller.h"
#include <apps/runtime/async_job_runner.h>
#include <cmath>
#include <mutex>
#include <optional>
#include <mooncake_log.h>

using namespace app_pps;

struct PpsController::Worker {
    enum class Action { Poll, Apply, Enable, Off };
    struct Command {
        Action action;
        float voltage     = 0;
        float current     = 0;
        uint64_t revision = 0;
    };

    Worker(std::shared_ptr<modules::I2cDevice> device, Clock clock)
        : device(std::move(device)), pps(*this->device), clock(std::move(clock))
    {
    }

    std::shared_ptr<modules::I2cDevice> device;
    pps::PpsDevice pps;
    Clock clock;
    mutable std::mutex mutex;
    PpsState state;
    std::optional<Command> command;
    uint64_t revision       = 0;
    bool working            = false;
    bool active             = true;
    bool pollRequested      = true;
    bool offRequested       = false;
    bool reconnectRequested = true;
    bool initialized        = false;

    bool interrupted()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return offRequested || reconnectRequested || !active;
    }

    PpsState snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto result = state;
        if (result.hasTelemetry && clock() - result.lastGoodPollMs > 1000) {
            result.telemetry.connected = false;
            result.output              = OutputState::Unknown;
            result.safetyReady         = false;
            result.telemetry.lastError = "PPS telemetry stale; output state unknown";
        }
        return result;
    }

    void failTelemetry(const std::string& error)
    {
        std::lock_guard<std::mutex> lock(mutex);
        state.telemetry.connected = false;
        state.telemetryFresh      = false;
        state.telemetry.lastError = error;
        state.identityValid       = false;
        state.output              = OutputState::Unknown;
        state.safetyReady         = false;
        initialized               = false;
    }

    bool writeOutput(bool enabled)
    {
        bool actual   = false;
        const bool ok = pps.setOutputEnabled(enabled) && pps.readOutputEnabled(actual) && actual == enabled;
        std::lock_guard<std::mutex> lock(mutex);
        state.output         = ok ? (enabled ? OutputState::Enabled : OutputState::Disabled) : OutputState::Unknown;
        state.off            = enabled ? (offRequested ? OffState::Pending : OffState::NotRequested)
                                       : (ok ? OffState::Confirmed : OffState::Unconfirmed);
        state.telemetryFresh = false;
        if (ok) {
            state.telemetry.outputEnabled = enabled;
        } else {
            state.safetyReady = false;
            initialized       = false;
        }
        return ok;
    }

    void poll()
    {
        if (interrupted()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            state.lastPollMs = clock();
        }
        if (!initialized) {
            pps::PpsIdentity identity;
            if (!pps.probe()) {
                failTelemetry("No PPS detected / check 9-36V input power");
                return;
            }
            if (!pps.readIdentity(identity, [this] { return interrupted(); })) {
                failTelemetry("PPS identity read failed; controls locked");
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                state.identity      = identity;
                state.identityValid = identity.moduleId == pps::EXPECTED_MODULE_ID;
            }
            if (identity.moduleId != pps::EXPECTED_MODULE_ID) {
                failTelemetry("Unexpected module ID at I2C 0x35");
                return;
            }
            if (interrupted()) {
                return;
            }
            if (!writeOutput(false)) {
                failTelemetry("Cannot confirm PPS output off; controls locked");
                return;
            }
            initialized = true;
        }

        pps::PpsTelemetry telemetry;
        if (!pps.readTelemetry(telemetry, [this] { return interrupted(); })) {
            // Retain last-good measurements, but never infer output-off from a read failure.
            failTelemetry(telemetry.lastError);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        const bool first     = !state.safetyReady;
        state.telemetry      = telemetry;
        state.output         = telemetry.outputEnabled ? OutputState::Enabled : OutputState::Disabled;
        state.safetyReady    = true;
        state.hasTelemetry   = true;
        state.telemetryFresh = true;
        state.lastGoodPollMs = clock();
        if (first && !state.setpointsDirty) {
            state.setpointsVerified = true;
        }
        if (!state.setpointsDirty) {
            state.pendingVoltage = telemetry.setpoints.voltage;
            state.pendingCurrent = telemetry.setpoints.current;
        }
    }

    void execute(const Command& request)
    {
        bool ok           = false;
        const char* error = "PPS command failed";
        if (request.action == Action::Off) {
            ok    = writeOutput(false);
            error = "Emergency Off not confirmed; output may still be live";
        } else if (request.action == Action::Enable) {
            auto current           = snapshot();
            current.commandPending = false;
            ok                     = PpsController::canEnable(current) && !interrupted() && writeOutput(true);
            error                  = "Enable not confirmed; output state unknown or setpoints unverified";
        } else {
            auto current           = snapshot();
            current.commandPending = false;
            error                  = "Setpoints not verified; may be partially updated. Reapply with output off";
            if (PpsController::canApply(current) && !interrupted()) {
                bool enabled = true;
                // Confirm hardware output-off again, rather than trusting cached UI state.
                ok = pps.readOutputEnabled(enabled) && !enabled;
                if (!ok) {
                    failTelemetry("Output is not confirmed off; setpoints unchanged");
                } else {
                    pps::PpsSetpoints actual;
                    ok = pps.writeSetpoints(request.voltage, request.current, [this] { return interrupted(); }) &&
                         !interrupted() && pps.readSetpoints(actual, [this] { return interrupted(); }) &&
                         std::fabs(actual.voltage - request.voltage) < 0.001f &&
                         std::fabs(actual.current - request.current) < 0.001f;
                }
            }
            std::lock_guard<std::mutex> lock(mutex);
            state.setpointsVerified = ok;
            if (ok) {
                state.customVoltage = request.voltage;
                state.customCurrent = request.current;
                if (revision == request.revision) {
                    state.setpointsDirty = false;
                }
            } else {
                state.setpointsDirty = true;
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            state.lastCommandMs    = clock();
            state.lastCommandError = ok ? "" : error;
            state.commandStatus    = ok ? (request.action == Action::Apply    ? "Setpoints verified"
                                           : request.action == Action::Enable ? "Output enabled"
                                                                              : "Output off confirmed")
                                        : "Command failed";
        }
    }

    void drain()
    {
        for (;;) {
            Command next{Action::Poll};
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (offRequested) {
                    offRequested = false;
                    command.reset();
                    next.action = Action::Off;
                } else if (reconnectRequested) {
                    reconnectRequested = false;
                    initialized        = false;
                    state.safetyReady  = false;
                    state.output       = OutputState::Unknown;
                    pollRequested      = false;
                } else if (command) {
                    next = *command;
                    command.reset();
                } else if (pollRequested && active) {
                    pollRequested = false;
                } else {
                    state.commandPending = false;
                    working              = false;
                    return;
                }
            }
            if (next.action == Action::Poll) {
                poll();
            } else {
                execute(next);
            }
        }
    }
};

std::shared_ptr<PpsController::Worker>& PpsController::defaultWorker()
{
    static std::shared_ptr<Worker> worker;
    return worker;
}

PpsController::PpsController()
    : PpsController([] {
          // One device owner survives app close/reopen until outstanding I2C work has finished.
          auto& worker = defaultWorker();
          if (!worker) {
              worker = std::make_shared<Worker>(
                  std::make_shared<modules::HalI2cDevice>(hal::ModuleBus::MBus, pps::DEFAULT_I2C_ADDRESS),
                  [] { return GetHAL()->millis(); });
          }
          return worker;
      }())
{
}

PpsController::PpsController(std::shared_ptr<modules::I2cDevice> device, Clock clock)
    : PpsController(std::make_shared<Worker>(std::move(device), std::move(clock)))
{
}

PpsController::PpsController(std::shared_ptr<Worker> worker) : _worker(std::move(worker))
{
    std::lock_guard<std::mutex> lock(_worker->mutex);
    _worker->active = true;
    _worker->command.reset();
    _worker->reconnectRequested = true;
    _worker->state.safetyReady  = false;
    _worker->state.output       = OutputState::Unknown;
}

PpsController::~PpsController()
{
    // Back cancels queued Apply/Enable and requests off without waiting under the LVGL lock.
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        _worker->active             = false;
        _worker->reconnectRequested = false;
        _worker->pollRequested      = false;
        _worker->command.reset();
        if (_worker->state.identity.moduleId == pps::EXPECTED_MODULE_ID) {
            _worker->offRequested         = true;
            _worker->state.off            = OffState::Pending;
            _worker->state.commandPending = true;
            _worker->state.output         = OutputState::Unknown;
            _worker->state.commandStatus  = "Disabling output...";
        }
    }
    kick();
}

void PpsController::kick()
{
    kick(_worker);
}

void PpsController::kick(const std::shared_ptr<Worker>& worker)
{
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (worker->working ||
            (!worker->offRequested && !worker->reconnectRequested && !worker->command && !worker->pollRequested)) {
            return;
        }
        worker->working = true;
    }
    // The worker gate serializes I2C; a job-local runner has no finish/restart race.
    auto runner = std::make_shared<app_runtime::AsyncJobRunner>();
    if (!runner->start("PPS I2C", [worker, runner] { worker->drain(); })) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->working                = false;
        worker->state.lastCommandError = "PPS worker unavailable; command remains pending";
    }
}

void PpsController::shutdown()
{
    auto worker = defaultWorker();
    if (!worker) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->active             = false;
        worker->reconnectRequested = false;
        worker->pollRequested      = false;
        worker->command.reset();
        // Automatic cleanup only writes to an identified PPS. Explicit Emergency Off remains unconditional.
        if (worker->state.identity.moduleId == pps::EXPECTED_MODULE_ID) {
            worker->offRequested         = true;
            worker->state.off            = OffState::Pending;
            worker->state.commandPending = true;
            worker->state.output         = OutputState::Unknown;
        }
    }
    kick(worker);
    // The process-wide bounded worker drain owns teardown waiting.
}

std::optional<PpsState> PpsController::backgroundState()
{
    auto worker = defaultWorker();
    return worker ? std::optional<PpsState>(worker->snapshot()) : std::nullopt;
}

std::string PpsController::outputSummary(const PpsState& state)
{
    if (state.off == OffState::Pending) return "Off pending";
    if (state.off == OffState::Unconfirmed) return "OFF UNCONFIRMED";
    if (state.output == OutputState::Enabled) return "Output ON";
    if (state.output == OutputState::Disabled) return "Output off";
    return "Output unknown";
}

void PpsController::update(uint32_t nowMs)
{
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        if (nowMs - _worker->state.lastPollMs >= 200) {
            _worker->pollRequested = true;
        }
    }
    kick();
}

PpsState PpsController::state() const
{
    return _worker->snapshot();
}

void PpsController::setPendingVoltage(float voltage)
{
    if (!std::isfinite(voltage)) {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        _worker->state.lastCommandError = "Invalid voltage rejected";
        mclog::tagWarn("pps-controller", "invalid voltage rejected");
        return;
    }
    if (voltage != pps::PpsDevice::clampVoltage(voltage)) {
        mclog::tagWarn("pps-controller", "voltage clamped to PPS limits");
    }
    std::lock_guard<std::mutex> lock(_worker->mutex);
    if (_worker->state.pendingVoltage == pps::PpsDevice::clampVoltage(voltage)) return;
    _worker->state.pendingVoltage = pps::PpsDevice::clampVoltage(voltage);
    _worker->state.setpointsDirty = true;
    ++_worker->revision;
}

void PpsController::setPendingCurrent(float current)
{
    if (!std::isfinite(current)) {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        _worker->state.lastCommandError = "Invalid current rejected";
        mclog::tagWarn("pps-controller", "invalid current rejected");
        return;
    }
    if (current != pps::PpsDevice::clampCurrent(current)) {
        mclog::tagWarn("pps-controller", "current clamped to PPS limits");
    }
    std::lock_guard<std::mutex> lock(_worker->mutex);
    if (_worker->state.pendingCurrent == pps::PpsDevice::clampCurrent(current)) return;
    _worker->state.pendingCurrent = pps::PpsDevice::clampCurrent(current);
    _worker->state.setpointsDirty = true;
    ++_worker->revision;
}

bool PpsController::canApply(const PpsState& state)
{
    return state.telemetry.connected && state.safetyReady && state.output == OutputState::Disabled &&
           !state.commandPending;
}

bool PpsController::canEnable(const PpsState& state)
{
    return canApply(state) && state.setpointsVerified && !state.setpointsDirty;
}

bool PpsController::applySetpoints(uint32_t)
{
    if (!canApply(state())) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        if (_worker->state.commandPending || _worker->offRequested) {
            return false;
        }
        _worker->command              = Worker::Command{Worker::Action::Apply, _worker->state.pendingVoltage,
                                           _worker->state.pendingCurrent, _worker->revision};
        _worker->state.commandPending = true;
        _worker->state.commandStatus  = "Applying setpoints...";
    }
    kick();
    return true;
}

bool PpsController::setOutputEnabled(bool enabled, uint32_t nowMs)
{
    if (!enabled) {
        return emergencyOff(nowMs);
    }
    if (!canEnable(state())) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        if (_worker->state.commandPending || _worker->offRequested) {
            return false;
        }
        _worker->command              = Worker::Command{Worker::Action::Enable};
        _worker->state.commandPending = true;
        _worker->state.commandStatus  = "Enabling output...";
    }
    kick();
    return true;
}

bool PpsController::emergencyOff(uint32_t)
{
    {
        std::lock_guard<std::mutex> lock(_worker->mutex);
        _worker->offRequested = true;
        _worker->state.off    = OffState::Pending;
        _worker->command.reset();
        _worker->state.commandPending = true;
        _worker->state.output         = OutputState::Unknown;
        _worker->state.commandStatus  = "Disabling output...";
    }
    kick();
    return true;
}

void PpsController::applyPreset(float voltage, float current)
{
    setPendingVoltage(voltage);
    setPendingCurrent(current);
}

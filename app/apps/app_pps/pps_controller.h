/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <modules/i2c_device.h>
#include <modules/pps/pps_device.h>

namespace app_pps {

enum class OutputState { Unknown, Disabled, Enabled };
enum class OffState { NotRequested, Pending, Unconfirmed, Confirmed };

struct PpsState {
    pps::PpsIdentity identity;
    pps::PpsTelemetry telemetry;
    OutputState output      = OutputState::Unknown;
    OffState off            = OffState::NotRequested;
    float pendingVoltage    = 5.0f;
    float pendingCurrent    = 1.0f;
    float customVoltage     = 5.0f;
    float customCurrent     = 1.0f;
    uint32_t lastPollMs     = 0;
    uint32_t lastGoodPollMs = 0;
    uint32_t lastCommandMs  = 0;
    bool identityValid      = false;
    bool hasTelemetry       = false;
    bool telemetryFresh     = false;
    bool safetyReady        = false;
    bool setpointsVerified  = false;
    bool setpointsDirty     = false;
    bool commandPending     = false;
    std::string commandStatus;
    std::string lastCommandError;
};

class PpsController {
public:
    using Clock = std::function<uint32_t()>;
    PpsController();
    PpsController(std::shared_ptr<modules::I2cDevice> device, Clock clock);
    ~PpsController();
    PpsController(const PpsController&) = delete;
    PpsController& operator=(const PpsController&) = delete;

    void update(uint32_t nowMs);
    PpsState state() const;

    void setPendingVoltage(float voltage);
    void setPendingCurrent(float current);
    bool applySetpoints(uint32_t nowMs);
    bool setOutputEnabled(bool enabled, uint32_t nowMs);
    bool emergencyOff(uint32_t nowMs);
    void applyPreset(float voltage, float current);
    static bool canApply(const PpsState& state);
    static bool canEnable(const PpsState& state);
    static void shutdown();
    // Read the surviving device owner without opening PPS or issuing hardware work.
    static std::optional<PpsState> backgroundState();
    static std::string outputSummary(const PpsState& state);

private:
    struct Worker;
    std::shared_ptr<Worker> _worker;
    explicit PpsController(std::shared_ptr<Worker> worker);
    static std::shared_ptr<Worker>& defaultWorker();
    static void kick(const std::shared_ptr<Worker>& worker);
    void kick();
};

}  // namespace app_pps

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <hal/hal.h>

namespace app_settings_network {

struct NetworkSnapshot {
    hal::HalBase::WifiStatus status;
    std::vector<hal::HalBase::WifiNetwork> networks;
    std::string detail = "Ready";
    bool busy = false;
    bool updateActive = false;
};

struct NetworkCallbacks {
    std::function<hal::HalBase::WifiStatus()> status;
    std::function<hal::HalBase::OtaStatus()> otaStatus;
    std::function<std::vector<hal::HalBase::WifiNetwork>()> scan;
    std::function<bool(const std::string&, const std::string&)> connect;
    std::function<void()> disconnect;
    std::function<void()> forgetCredentials;
    std::function<void(bool)> setExtAntenna;
    std::function<bool()> startAp;
    std::function<void()> stopAp;
    std::function<bool(const std::string&, std::function<void()>)> startJob;
};

class NetworkController {
public:
    explicit NetworkController(NetworkCallbacks callbacks);

    NetworkSnapshot refresh();
    bool scan();
    bool connect(const std::string& ssid, const std::string& password);
    bool disconnect();
    bool forgetCredentials();
    void setExtAntennaEnabled(bool enabled);
    bool startAccessPoint();
    bool stopAccessPoint();

private:
    struct JobState {
        std::mutex mutex;
        bool complete = false;
        bool hasNetworks = false;
        std::vector<hal::HalBase::WifiNetwork> networks;
        std::string detail;
    };

    bool startCommand(const std::string& jobName, const std::string& pendingDetail, std::function<std::string()> work);
    void finishCompletedJob();
    static std::string scanDetail(size_t count);
    bool updateActive() const;

    NetworkCallbacks _callbacks;
    NetworkSnapshot _snapshot;
    std::shared_ptr<JobState> _job;
};

}  // namespace app_settings_network

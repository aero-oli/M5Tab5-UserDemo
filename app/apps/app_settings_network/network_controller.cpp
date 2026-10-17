/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "network_controller.h"

#include <utility>

using namespace app_settings_network;

NetworkController::NetworkController(NetworkCallbacks callbacks) : _callbacks(std::move(callbacks))
{
}

NetworkSnapshot NetworkController::refresh()
{
    finishCompletedJob();
    if (_callbacks.status) {
        _snapshot.status = _callbacks.status();
    }
    _snapshot.busy         = _job != nullptr;
    _snapshot.updateActive = updateActive();
    return _snapshot;
}

bool NetworkController::scan()
{
    if (updateActive()) {
        _snapshot.detail = "Firmware update owns Wi-Fi.";
        return false;
    }
    if (!_callbacks.scan) {
        _snapshot.detail = "Wi-Fi scan is unavailable.";
        return false;
    }
    if (!_callbacks.startJob) {
        _snapshot.detail = "Background jobs are unavailable.";
        return false;
    }
    if (_job != nullptr) {
        _snapshot.detail = "Another background task is already running. Try again in a moment.";
        return false;
    }

    auto scanFn = _callbacks.scan;
    auto state  = std::make_shared<JobState>();
    auto job    = [state, scanFn] {
        auto networks = scanFn();
        std::lock_guard<std::mutex> lock(state->mutex);
        state->hasNetworks = true;
        state->networks    = std::move(networks);
        state->detail      = scanDetail(state->networks.size());
        state->complete    = true;
    };

    if (!_callbacks.startJob("Wi-Fi scan", std::move(job))) {
        _snapshot.detail = "Another background task is already running. Try again in a moment.";
        return false;
    }

    _job             = state;
    _snapshot.detail = "Scanning for Wi-Fi networks...";
    _snapshot.busy   = true;
    return true;
}

bool NetworkController::connect(const std::string& ssid, const std::string& password)
{
    if (ssid.empty()) {
        _snapshot.detail = "SSID is required.";
        return false;
    }
    if (!_callbacks.connect) {
        _snapshot.detail = "Wi-Fi connect is unavailable.";
        return false;
    }
    auto connectFn = _callbacks.connect;
    return startCommand("Wi-Fi connect", "Connecting to " + ssid + "...", [connectFn, ssid, password] {
        const bool ok = connectFn(ssid, password);
        return ok ? "Connected to " + ssid + "." : "Connection failed.";
    });
}

bool NetworkController::disconnect()
{
    if (!_callbacks.disconnect) {
        _snapshot.detail = "Wi-Fi disconnect is unavailable.";
        return false;
    }
    auto disconnectFn = _callbacks.disconnect;
    return startCommand("Wi-Fi disconnect", "Disconnecting...", [disconnectFn] {
        disconnectFn();
        return std::string("Disconnected.");
    });
}

bool NetworkController::forgetCredentials()
{
    if (!_callbacks.forgetCredentials) {
        _snapshot.detail = "Forget credentials is unavailable.";
        return false;
    }
    auto forgetFn = _callbacks.forgetCredentials;
    return startCommand("Wi-Fi forget", "Clearing saved credentials...", [forgetFn] {
        forgetFn();
        return std::string("Saved credentials cleared.");
    });
}

void NetworkController::setExtAntennaEnabled(bool enabled)
{
    if (!_callbacks.setExtAntenna) return;
    auto set = _callbacks.setExtAntenna;
    startCommand("Wi-Fi antenna", "Changing antenna...", [set, enabled] {
        set(enabled);
        return std::string("Antenna request complete.");
    });
}

bool NetworkController::startAccessPoint()
{
    if (!_callbacks.startAp) {
        _snapshot.detail = "Access point start is unavailable.";
        return false;
    }
    auto startFn = _callbacks.startAp;
    return startCommand("Wi-Fi AP start", "Starting access point...",
                        [startFn] { return startFn() ? "Access point started." : "Access point start failed."; });
}

bool NetworkController::stopAccessPoint()
{
    if (!_callbacks.stopAp) {
        _snapshot.detail = "Access point stop is unavailable.";
        return false;
    }
    auto stopFn = _callbacks.stopAp;
    return startCommand("Wi-Fi AP stop", "Stopping access point...", [stopFn] {
        stopFn();
        return std::string("Access point stopped.");
    });
}

bool NetworkController::startCommand(const std::string& jobName, const std::string& pendingDetail,
                                     std::function<std::string()> work)
{
    if (updateActive()) {
        _snapshot.detail = "Firmware update owns Wi-Fi.";
        return false;
    }
    if (!_callbacks.startJob) {
        _snapshot.detail = "Background jobs are unavailable.";
        return false;
    }
    if (_job != nullptr) {
        _snapshot.detail = "Another background task is already running. Try again in a moment.";
        return false;
    }

    auto state = std::make_shared<JobState>();
    auto job   = [state, work = std::move(work)] {
        std::string detail = work();
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!detail.empty()) {
            state->detail = std::move(detail);
        }
        state->complete = true;
    };

    if (!_callbacks.startJob(jobName, std::move(job))) {
        _snapshot.detail = "Another background task is already running. Try again in a moment.";
        return false;
    }

    _job             = state;
    _snapshot.detail = pendingDetail;
    _snapshot.busy   = true;
    return true;
}

void NetworkController::finishCompletedJob()
{
    if (!_job) {
        return;
    }

    std::vector<hal::HalBase::WifiNetwork> networks;
    std::string detail;
    bool complete    = false;
    bool hasNetworks = false;
    {
        std::lock_guard<std::mutex> lock(_job->mutex);
        complete = _job->complete;
        if (!complete) {
            return;
        }
        hasNetworks = _job->hasNetworks;
        networks    = _job->networks;
        detail      = _job->detail;
    }

    if (hasNetworks) {
        _snapshot.networks = std::move(networks);
    }
    if (!detail.empty()) {
        _snapshot.detail = std::move(detail);
    }
    _job.reset();
    _snapshot.busy = false;
}

std::string NetworkController::scanDetail(size_t count)
{
    if (count == 0) {
        return "No Wi-Fi networks found.";
    }
    if (count == 1) {
        return "Found 1 Wi-Fi network.";
    }
    return "Found " + std::to_string(count) + " Wi-Fi networks.";
}

bool NetworkController::updateActive() const
{
    if (!_callbacks.otaStatus) return false;
    using State      = hal::HalBase::OtaState;
    const auto state = _callbacks.otaStatus().state;
    return state == State::Checking || state == State::Downloading || state == State::Installing ||
           state == State::Cancelling;
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "camera_controller.h"

#include <utility>

using namespace app_media_camera;

CameraController::CameraController(CameraCallbacks callbacks) : _callbacks(std::move(callbacks))
{
}

bool CameraController::start(lv_obj_t* canvas)
{
    if (canvas == nullptr || !canStart() || !_callbacks.start) {
        return false;
    }
    return _callbacks.start(canvas);
}

void CameraController::stop()
{
    if (canStop() && _callbacks.stop) {
        _callbacks.stop();
    }
}

void CameraController::stopOnClose()
{
    // Detach even if capture failed before the UI observed its new state.
    if (_callbacks.stop) {
        _callbacks.stop();
    }
}

hal::HalBase::CameraStatus CameraController::status() const
{
    return _callbacks.status ? _callbacks.status() : hal::HalBase::CameraStatus{};
}

bool CameraController::canStart() const
{
    return canStart(status().state);
}

bool CameraController::canStop() const
{
    return canStop(status().state);
}

bool CameraController::isRunning() const
{
    return status().state == hal::HalBase::CameraState::Running;
}

std::string CameraController::statusText() const
{
    return stateText(status().state);
}

bool CameraController::canStart(hal::HalBase::CameraState state)
{
    return state == hal::HalBase::CameraState::Stopped || state == hal::HalBase::CameraState::Failed;
}

bool CameraController::canStop(hal::HalBase::CameraState state)
{
    return state == hal::HalBase::CameraState::Starting || state == hal::HalBase::CameraState::Running;
}

const char* CameraController::stateText(hal::HalBase::CameraState state)
{
    using State = hal::HalBase::CameraState;
    switch (state) {
        case State::Stopped:
            return "Camera stopped";
        case State::Starting:
            return "Starting camera";
        case State::Running:
            return "Camera running";
        case State::Stopping:
            return "Stopping camera";
        case State::Failed:
            return "Camera failed";
        default:
            return "Camera unavailable";
    }
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>
#include <hal/hal.h>

namespace app_media_camera {

struct CameraCallbacks {
    std::function<bool(lv_obj_t*)> start;
    std::function<void()> stop;
    std::function<hal::HalBase::CameraStatus()> status;
};

class CameraController {
public:
    explicit CameraController(CameraCallbacks callbacks);

    bool start(lv_obj_t* canvas);
    void stop();
    void stopOnClose();
    hal::HalBase::CameraStatus status() const;
    bool canStart() const;
    bool canStop() const;
    bool isRunning() const;
    std::string statusText() const;
    static bool canStart(hal::HalBase::CameraState state);
    static bool canStop(hal::HalBase::CameraState state);
    static const char* stateText(hal::HalBase::CameraState state);

private:
    CameraCallbacks _callbacks;
};

}  // namespace app_media_camera

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_desktop.h"
#include <cstdlib>
#include <string>

namespace {

class DisplayLock {
public:
    explicit DisplayLock(HalDesktop& hal) : _hal(hal) { _hal.lvglLock(); }
    ~DisplayLock() { _hal.lvglUnlock(); }
private:
    HalDesktop& _hal;
};

bool cameraFault(const char* value)
{
    const char* fault = std::getenv("M5TAB5_DESKTOP_CAMERA_FAIL");
    return fault != nullptr && std::string(fault) == value;
}

}  // namespace

HalDesktop::~HalDesktop()
{
    stopCameraCapture();
}

bool HalDesktop::startCameraCapture(lv_obj_t* canvas)
{
    DisplayLock displayLock(*this);
    std::lock_guard<std::mutex> lock(_cameraMutex);
    if (_cameraTimer != nullptr) {
        return false;
    }
    _cameraStatus = {};
    _cameraStatus.simulated = true;
    if (canvas == nullptr || cameraFault("start")) {
        _cameraStatus.state = CameraState::Failed;
        _cameraStatus.lastError = "Simulated camera startup failure";
        return false;
    }
    lv_obj_update_layout(canvas);
    const int width = lv_obj_get_width(canvas);
    const int height = lv_obj_get_height(canvas);
    if (width <= 0 || height <= 0 || width > 1280 || height > 720) {
        _cameraStatus.state = CameraState::Failed;
        _cameraStatus.lastError = "Invalid camera preview dimensions";
        return false;
    }
    const size_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
    for (auto& buffer : _cameraBuffers) {
        buffer.assign(stride * height / sizeof(uint16_t), 0);
    }
    _cameraCanvas = canvas;
    _cameraStatus.state = CameraState::Starting;
    _cameraStatus.width = width;
    _cameraStatus.height = height;
    _cameraStartedMs = millis();
    _cameraNextBuffer = 0;
    _cameraTimer = lv_timer_create([](lv_timer_t* timer) {
        static_cast<HalDesktop*>(lv_timer_get_user_data(timer))->updateCameraFrame();
    }, 33, this);
    if (_cameraTimer == nullptr) {
        detachCamera();
        _cameraStatus.state = CameraState::Failed;
        _cameraStatus.lastError = "Could not start simulated camera";
        return false;
    }
    return true;
}

void HalDesktop::detachCamera()
{
    if (_cameraTimer != nullptr) {
        lv_timer_delete(_cameraTimer);
        _cameraTimer = nullptr;
    }
    if (_cameraCanvas != nullptr) {
        if (const void* source = lv_image_get_src(_cameraCanvas)) {
            lv_image_cache_drop(source);
        }
        lv_image_set_src(_cameraCanvas, nullptr);
        _cameraCanvas = nullptr;
    }
    for (auto& buffer : _cameraBuffers) {
        std::vector<uint16_t>().swap(buffer);
    }
}

void HalDesktop::stopCameraCapture()
{
    DisplayLock displayLock(*this);
    std::lock_guard<std::mutex> lock(_cameraMutex);
    if (_cameraTimer == nullptr && _cameraCanvas == nullptr) {
        return;
    }
    detachCamera();
    _cameraStatus.state = CameraState::Stopped;
    _cameraStatus.lastError.clear();
}

hal::HalBase::CameraStatus HalDesktop::cameraGetStatus()
{
    std::lock_guard<std::mutex> lock(_cameraMutex);
    return _cameraStatus;
}

void HalDesktop::updateCameraFrame()
{
    // LVGL invokes this while holding the display lock.
    std::lock_guard<std::mutex> lock(_cameraMutex);
    if (cameraFault("stall")) {
        if (millis() - _cameraStartedMs < 2000) {
            return;
        }
        detachCamera();
        _cameraStatus.state = CameraState::Failed;
        _cameraStatus.lastError = "Camera frame timeout";
        return;
    }
    if (cameraFault("frame") && _cameraStatus.frames >= 2) {
        detachCamera();
        _cameraStatus.state = CameraState::Failed;
        _cameraStatus.lastError = "Simulated camera frame failure";
        return;
    }
    const int width = _cameraStatus.width;
    const int height = _cameraStatus.height;
    const int stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565) / sizeof(uint16_t);
    auto& buffer = _cameraBuffers[_cameraNextBuffer];
    const uint16_t colors[] = {0x2189, 0x2CCB, 0xCC89, 0xB34A};
    const int line = static_cast<int>(_cameraStatus.frames * 9 % width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            buffer[static_cast<size_t>(y) * stride + x] =
                x >= line && x < line + 8 ? 0xFFFF : colors[x * 4 / width];
        }
    }
    lv_canvas_set_buffer(_cameraCanvas, buffer.data(), width, height, LV_COLOR_FORMAT_RGB565);
    _cameraNextBuffer ^= 1;
    _cameraStatus.state = CameraState::Running;
    ++_cameraStatus.frames;
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <hal/camera_preview.h>
#include <array>
#include <memory>
#include <mutex>
#include <new>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bsp/esp-bsp.h>
#include <driver/ppa.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_video_device.h>
#include <esp_video_init.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <linux/videodev2.h>

namespace {

using State = hal::HalBase::CameraState;
constexpr const char* kTag = "camera";
constexpr int kBufferCount = 2;
constexpr int64_t kFrameTimeoutUs = 2000000;
constexpr size_t kBufferAlignment = 128;

std::mutex s_mutex;
hal::HalBase::CameraStatus s_status{State::Stopped, {}, 0, 0, 0, false};
TaskHandle_t s_task = nullptr;
// Target access always takes the display lock before s_mutex.
lv_obj_t* s_canvas = nullptr;
bool s_stop_requested = false;
bool s_waiting_for_frame = false;
bool s_frame_timed_out = false;
int64_t s_frame_wait_started_us = 0;
bool s_video_initialized = false;

struct CaptureRequest {
    int width;
    int height;
};

bool stopping()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_stop_requested;
}

void clear_canvas(lv_obj_t* canvas)
{
    if (const void* source = lv_image_get_src(canvas)) {
        lv_image_cache_drop(source);
    }
    lv_image_set_src(canvas, nullptr);
}

void detach_target()
{
    LvglLockGuard displayLock;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_canvas != nullptr) {
        clear_canvas(s_canvas);
        s_canvas = nullptr;
    }
}

void frame_watchdog(void*)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_task == nullptr || !s_waiting_for_frame) {
        return;
    }
    if (esp_timer_get_time() - s_frame_wait_started_us >= kFrameTimeoutUs) {
        s_frame_timed_out = true;
    }
    if (s_stop_requested || s_frame_timed_out) {
        // DQBUF waits on a semaphore indefinitely. Repeating also covers
        // Stop arriving just before the capture task enters that wait.
        xTaskAbortDelay(s_task);
    }
}

class CaptureResources {
public:
    CaptureResources(std::string& error, bool& cleanupFailed) : _error(error), _cleanupFailed(cleanupFailed) {}

    ~CaptureResources()
    {
        if (_watchdog != nullptr) {
            esp_timer_stop(_watchdog);
            if (esp_timer_delete(_watchdog) != ESP_OK) {
                cleanup_error("Could not release camera watchdog");
            }
        }
        if (_fd >= 0) {
            if (_streaming) {
                int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (ioctl(_fd, VIDIOC_STREAMOFF, &type) != 0) {
                    cleanup_error("Could not stop camera stream");
                }
            }
            for (size_t i = 0; i < _captureBuffers.size(); ++i) {
                if (_captureBuffers[i] != nullptr) {
                    munmap(_captureBuffers[i], _captureLengths[i]);
                }
            }
            if (close(_fd) != 0) {
                cleanup_error("Could not close camera device");
            }
        }
        if (_ppa != nullptr) {
            if (ppa_unregister_client(_ppa) != ESP_OK) {
                cleanup_error("Could not release camera scaler");
            }
        }
        for (auto* buffer : _previewBuffers) {
            heap_caps_free(buffer);
        }
    }

    bool initialize(const CaptureRequest& request, std::string& error)
    {
        if (!s_video_initialized) {
            esp_video_init_csi_config_t csi = {};
            csi.sccb_config.init_sccb = false;
            csi.sccb_config.i2c_handle = bsp_i2c_get_handle();
            csi.sccb_config.freq = 400000;
            csi.reset_pin = -1;
            csi.pwdn_pin = -1;
            esp_video_init_config_t config = {};
            config.csi = &csi;
            if (esp_video_init(&config) != ESP_OK) {
                error = "Camera initialization failed";
                return false;
            }
            s_video_initialized = true;
        }
        if (stopping()) {
            return false;
        }
        _fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
        if (_fd < 0) {
            error = "Camera unavailable: could not open device";
            return false;
        }
        v4l2_format format = {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_fd, VIDIOC_G_FMT, &format) != 0) {
            error = "Could not read camera format";
            return false;
        }
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
        if (ioctl(_fd, VIDIOC_S_FMT, &format) != 0 || ioctl(_fd, VIDIOC_G_FMT, &format) != 0 ||
            format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 || format.fmt.pix.width == 0 ||
            format.fmt.pix.height == 0) {
            error = "Camera RGB565 format is unavailable";
            return false;
        }
        _sourceWidth = format.fmt.pix.width;
        _sourceHeight = format.fmt.pix.height;
        v4l2_requestbuffers buffers = {};
        buffers.count = kBufferCount;
        buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffers.memory = V4L2_MEMORY_MMAP;
        if (ioctl(_fd, VIDIOC_REQBUFS, &buffers) != 0 || buffers.count < kBufferCount) {
            error = "Could not allocate camera capture buffers";
            return false;
        }
        for (int i = 0; i < kBufferCount; ++i) {
            v4l2_buffer buffer = {};
            buffer.index = i;
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            if (ioctl(_fd, VIDIOC_QUERYBUF, &buffer) != 0 ||
                buffer.length < static_cast<size_t>(_sourceWidth) * _sourceHeight * sizeof(uint16_t)) {
                error = "Invalid camera capture buffer";
                return false;
            }
            void* mapped = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, buffer.m.offset);
            if (mapped == nullptr) {
                error = "Could not map camera capture buffer";
                return false;
            }
            _captureBuffers[i] = static_cast<uint8_t*>(mapped);
            _captureLengths[i] = buffer.length;
            if (ioctl(_fd, VIDIOC_QBUF, &buffer) != 0) {
                error = "Could not queue camera capture buffer";
                return false;
            }
        }

        _request = request;
        _previewBytes = (static_cast<size_t>(request.width) * request.height * sizeof(uint16_t) +
                         kBufferAlignment - 1) & ~(kBufferAlignment - 1);
        for (auto& buffer : _previewBuffers) {
            buffer = static_cast<uint8_t*>(heap_caps_aligned_calloc(kBufferAlignment, 1, _previewBytes,
                                                                   MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM));
            if (buffer == nullptr) {
                error = "Not enough memory for camera preview";
                return false;
            }
        }
        ppa_client_config_t ppaConfig = {};
        ppaConfig.oper_type = PPA_OPERATION_SRM;
        ppaConfig.max_pending_trans_num = 1;
        if (ppa_register_client(&ppaConfig, &_ppa) != ESP_OK) {
            error = "Camera preview scaler is unavailable";
            return false;
        }
        esp_timer_create_args_t timer = {};
        timer.callback = frame_watchdog;
        timer.name = "camera_watchdog";
        if (esp_timer_create(&timer, &_watchdog) != ESP_OK || esp_timer_start_periodic(_watchdog, 50000) != ESP_OK) {
            error = "Could not start camera frame watchdog";
            return false;
        }
        if (stopping()) {
            return false;
        }
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_fd, VIDIOC_STREAMON, &type) != 0) {
            error = "Could not start camera stream";
            return false;
        }
        _streaming = true;
        return true;
    }

    void run(std::string& error)
    {
        const auto fit = hal::fitCameraPreview(_sourceWidth, _sourceHeight, _request.width, _request.height);
        if (!fit.valid()) {
            error = "Camera frame cannot fit in the preview";
            return;
        }
        int nextBuffer = 0;
        while (!stopping()) {
            v4l2_buffer frame = {};
            frame.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            frame.memory = V4L2_MEMORY_MMAP;
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                if (s_stop_requested) {
                    break;
                }
                s_waiting_for_frame = true;
                s_frame_timed_out = false;
                s_frame_wait_started_us = esp_timer_get_time();
            }
            const int received = ioctl(_fd, VIDIOC_DQBUF, &frame);
            bool timedOut;
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_waiting_for_frame = false;
                timedOut = s_frame_timed_out;
            }
            if (stopping()) {
                break;
            }
            if (received != 0 || timedOut) {
                error = timedOut ? "Camera frame timeout" : "Could not receive camera frame";
                break;
            }
            if (frame.index >= kBufferCount || (frame.flags & V4L2_BUF_FLAG_ERROR) != 0 ||
                frame.bytesused < static_cast<size_t>(_sourceWidth) * _sourceHeight * sizeof(uint16_t)) {
                error = "Camera returned an invalid frame";
                break;
            }
            ppa_srm_oper_config_t transform = {};
            transform.in.buffer = _captureBuffers[frame.index];
            transform.in.pic_w = _sourceWidth;
            transform.in.pic_h = _sourceHeight;
            transform.in.block_w = _sourceWidth;
            transform.in.block_h = _sourceHeight;
            transform.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
            transform.out.buffer = _previewBuffers[nextBuffer];
            transform.out.buffer_size = _previewBytes;
            transform.out.pic_w = _request.width;
            transform.out.pic_h = _request.height;
            transform.out.block_offset_x = fit.x;
            transform.out.block_offset_y = fit.y;
            transform.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
            transform.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
            transform.scale_x = fit.scale;
            transform.scale_y = fit.scale;
            transform.mirror_x = true;
            transform.mode = PPA_TRANS_MODE_BLOCKING;
            const esp_err_t transformed = ppa_do_scale_rotate_mirror(_ppa, &transform);
            if (ioctl(_fd, VIDIOC_QBUF, &frame) != 0 || transformed != ESP_OK) {
                error = transformed != ESP_OK ? "Could not scale camera frame" : "Could not recycle camera frame";
                break;
            }
            {
                LvglLockGuard displayLock;
                std::lock_guard<std::mutex> lock(s_mutex);
                if (s_stop_requested || s_canvas == nullptr) {
                    break;
                }
                // LVGL reads one buffer while the next frame is transformed into the other.
                lv_canvas_set_buffer(s_canvas, _previewBuffers[nextBuffer], _request.width, _request.height,
                                     LV_COLOR_FORMAT_RGB565);
                s_status.state = State::Running;
                ++s_status.frames;
            }
            nextBuffer ^= 1;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

private:
    void cleanup_error(const char* message)
    {
        ESP_LOGE(kTag, "%s", message);
        _cleanupFailed = true;
        if (_error.empty()) {
            _error = message;
        }
    }

    std::string& _error;
    bool& _cleanupFailed;
    int _fd = -1;
    bool _streaming = false;
    uint32_t _sourceWidth = 0;
    uint32_t _sourceHeight = 0;
    CaptureRequest _request{};
    std::array<uint8_t*, kBufferCount> _captureBuffers{};
    std::array<size_t, kBufferCount> _captureLengths{};
    std::array<uint8_t*, 2> _previewBuffers{};
    size_t _previewBytes = 0;
    ppa_client_handle_t _ppa = nullptr;
    esp_timer_handle_t _watchdog = nullptr;
};

void run_capture_session(CaptureRequest* param)
{
    std::unique_ptr<CaptureRequest> request(param);
    std::string error;
    bool cleanupFailed = false;
    {
        CaptureResources resources(error, cleanupFailed);
        if (resources.initialize(*request, error)) {
            resources.run(error);
        }
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_waiting_for_frame = false;
        }
        // Detach before releasing the buffers or allowing another session to start.
        detach_target();
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_status.state = cleanupFailed ? State::Unavailable :
                         error.empty() || s_stop_requested ? State::Stopped : State::Failed;
        s_status.lastError = s_stop_requested && !cleanupFailed ? "" : error;
        s_task = nullptr;
    }
}

void capture_task(void* param)
{
    // Return through normal C++ scope cleanup before FreeRTOS deletes this task.
    run_capture_session(static_cast<CaptureRequest*>(param));
    vTaskDelete(nullptr);
}

}  // namespace

bool HalEsp32::startCameraCapture(lv_obj_t* canvas)
{
    LvglLockGuard displayLock;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_task != nullptr || s_status.state == State::Unavailable) {
        return false;
    }
    s_status = {};
    if (canvas == nullptr) {
        s_status.state = State::Failed;
        s_status.lastError = "Camera preview is unavailable";
        return false;
    }
    lv_obj_update_layout(canvas);
    const int width = lv_obj_get_width(canvas);
    const int height = lv_obj_get_height(canvas);
    if (width <= 0 || height <= 0 || width > 1280 || height > 720) {
        s_status.state = State::Failed;
        s_status.lastError = "Invalid camera preview dimensions";
        return false;
    }
    auto* request = new (std::nothrow) CaptureRequest{width, height};
    if (request == nullptr) {
        s_status.state = State::Failed;
        s_status.lastError = "Not enough memory to start camera";
        return false;
    }
    s_canvas = canvas;
    s_stop_requested = false;
    s_status.state = State::Starting;
    s_status.width = width;
    s_status.height = height;
    if (xTaskCreatePinnedToCore(capture_task, "camera", 8192, request, 5, &s_task, 1) != pdPASS) {
        s_task = nullptr;
        s_canvas = nullptr;
        s_status.state = State::Failed;
        s_status.lastError = "Could not start camera task";
        delete request;
        return false;
    }
    return true;
}

void HalEsp32::stopCameraCapture()
{
    LvglLockGuard displayLock;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_canvas != nullptr) {
        clear_canvas(s_canvas);
        s_canvas = nullptr;
    }
    if (s_task != nullptr) {
        s_stop_requested = true;
        s_status.state = State::Stopping;
    }
}

hal::HalBase::CameraStatus HalEsp32::cameraGetStatus()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_status;
}

bool HalEsp32::isCameraCapturing()
{
    return cameraGetStatus().state == State::Running;
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_camera.h"
#include "camera_controller.h"
#include "camera_layout.h"

#include <memory>
#include <cstdio>
#include <string>
#include <utility>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_media_camera;

namespace {

class AppCamera : public mooncake::AppAbility {
public:
    AppCamera() : _controller(makeCallbacks())
    {
        setAppInfo().name = "Camera";
    }

    ~AppCamera() override
    {
        _controller.stopOnClose();
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Camera", [&]() { close(); });
        _shell->init();
        build();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 100) {
            return;
        }
        LvglLockGuard lock;
        refresh();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _controller.stopOnClose();
        _shell.reset();
    }

private:
    CameraController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _cameraCanvas = nullptr;
    lv_obj_t* _status = nullptr;
    lv_obj_t* _startButton = nullptr;
    lv_obj_t* _stopButton = nullptr;
    lv_obj_t* _detail = nullptr;
    lv_obj_t* _error = nullptr;
    std::string _lastStatus;
    std::string _lastDetail;
    std::string _lastError;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        _lastStatus.clear();
        _lastDetail.clear();
        _lastError.clear();
        const auto layout = makeCameraLayout();

        auto* previewPanel = app_ui::makePanel(content(), layout.previewPanel.x, layout.previewPanel.y,
                                               layout.previewPanel.w, layout.previewPanel.h);
        _cameraCanvas = lv_canvas_create(previewPanel);
        lv_obj_set_pos(_cameraCanvas, layout.cameraCanvas.x, layout.cameraCanvas.y);
        lv_obj_set_size(_cameraCanvas, layout.cameraCanvas.w, layout.cameraCanvas.h);
        lv_obj_set_style_bg_color(_cameraCanvas, app_ui::theme::color(app_ui::theme::bg), 0);
        lv_obj_set_style_radius(_cameraCanvas, 8, 0);

        auto* sidePanel =
            app_ui::makePanel(content(), layout.controlPanel.x, layout.controlPanel.y, layout.controlPanel.w,
                              layout.controlPanel.h);
        auto* title = app_ui::makeLabel(sidePanel, "Capture", layout.controlTitle.x, layout.controlTitle.y,
                                        &lv_font_montserrat_24, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.controlTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _status = app_ui::makeLabel(sidePanel, "Camera stopped", layout.statusText.x, layout.statusText.y,
                                    &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_status, layout.statusText.w);
        lv_label_set_long_mode(_status, LV_LABEL_LONG_WRAP);

        _startButton = app_ui::makeButton(sidePanel, "Start", layout.startButton.x, layout.startButton.y,
                                         layout.startButton.w, layout.startButton.h, app_ui::ButtonKind::Primary);
        lv_obj_add_event_cb(_startButton, AppCamera::onStart, LV_EVENT_CLICKED, this);
        _stopButton = app_ui::makeButton(sidePanel, "Stop", layout.stopButton.x, layout.stopButton.y,
                                        layout.stopButton.w, layout.stopButton.h, app_ui::ButtonKind::Danger);
        lv_obj_add_event_cb(_stopButton, AppCamera::onStop, LV_EVENT_CLICKED, this);

        _detail = app_ui::makeLabel(content(), "",
                                       layout.hint.x, layout.hint.y,
                                       &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_size(_detail, layout.hint.w, layout.hint.h);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        _error = app_ui::makeFittedLabel(content(), "", layout.errorText.x, layout.errorText.y,
                                         layout.errorText.w, layout.errorText.h, &lv_font_montserrat_18,
                                         app_ui::theme::color(app_ui::theme::red), LV_LABEL_LONG_WRAP,
                                         LV_TEXT_ALIGN_LEFT);
        refresh();
    }

    void refresh()
    {
        const auto snapshot = _controller.status();
        const auto text = CameraController::stateText(snapshot.state);
        if (_lastStatus != text) {
            lv_label_set_text(_status, text);
            _lastStatus = text;
        }
        if (CameraController::canStart(snapshot.state)) {
            lv_obj_remove_state(_startButton, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(_startButton, LV_STATE_DISABLED);
        }
        if (CameraController::canStop(snapshot.state)) {
            lv_obj_remove_state(_stopButton, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(_stopButton, LV_STATE_DISABLED);
        }
        char detail[80] = {};
        if (snapshot.width > 0 && snapshot.height > 0) {
            std::snprintf(detail, sizeof(detail), "%s%d x %d\n%u frames", snapshot.simulated ? "Simulation\n" : "",
                          snapshot.width, snapshot.height,
                          static_cast<unsigned>(snapshot.frames));
        }
        if (_lastDetail != detail) {
            lv_label_set_text(_detail, detail);
            _lastDetail = detail;
        }
        if (_lastError != snapshot.lastError) {
            lv_label_set_text(_error, snapshot.lastError.c_str());
            _lastError = snapshot.lastError;
        }
    }

    void start()
    {
        audio::play_next_tone_progression();
        _controller.start(_cameraCanvas);
        refresh();
    }

    void stop()
    {
        audio::play_next_tone_progression();
        _controller.stop();
        refresh();
    }

    static CameraCallbacks makeCallbacks()
    {
        CameraCallbacks callbacks;
        callbacks.start = [](lv_obj_t* canvas) { return GetHAL()->startCameraCapture(canvas); };
        callbacks.stop = [] { GetHAL()->stopCameraCapture(); };
        callbacks.status = [] { return GetHAL()->cameraGetStatus(); };
        return callbacks;
    }

    static void onStart(lv_event_t* event)
    {
        auto* app = static_cast<AppCamera*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->start();
        }
    }

    static void onStop(lv_event_t* event)
    {
        auto* app = static_cast<AppCamera*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->stop();
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_media_camera::makeCameraApp()
{
    return std::make_unique<AppCamera>();
}

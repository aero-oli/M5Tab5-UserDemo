/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_audio_tool.h"
#include "audio_tool_controller.h"
#include "audio_tool_layout.h"

#include <memory>
#include <string>
#include <utility>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_media_audio;

namespace {

struct AudioToolConfig {
    AudioToolKind kind = AudioToolKind::DualMic;
    const char* title = "";
    const char* description = "";
    bool hasStop = false;
    AudioToolCallbacks callbacks;
};

class AppAudioTool : public mooncake::AppAbility {
public:
    explicit AppAudioTool(AudioToolConfig config)
        : _config(std::move(config)), _controller(_config.kind, std::move(_config.callbacks))
    {
        setAppInfo().name = _config.title;
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>(_config.title, [&]() { close(); });
        _shell->init();
        build();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 250) {
            return;
        }
        LvglLockGuard lock;
        refresh();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        if (_controller.shouldStopOnClose()) {
            _controller.stop();
        }
        _shell.reset();
    }

private:
    AudioToolConfig _config;
    AudioToolController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _state = nullptr;
    lv_obj_t* _start = nullptr;
    lv_obj_t* _detail = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeAudioToolLayout(_config.hasStop);
        auto* description = app_ui::makeLabel(content(), _config.description, layout.description.x,
                                              layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);

        _state = app_ui::makeMetricCard(content(), "State", "Idle", layout.stateCard.x, layout.stateCard.y,
                                        layout.stateCard.w, layout.stateCard.h);
        _start = app_ui::makeButton(content(), _config.kind == AudioToolKind::Music ? "Play" : "Record and Play",
                                         layout.startButton.x, layout.startButton.y, layout.startButton.w,
                                         layout.startButton.h, app_ui::ButtonKind::Primary);
        lv_obj_add_event_cb(_start, AppAudioTool::onStart, LV_EVENT_CLICKED, this);
        if (layout.hasStopButton) {
            auto* stop = app_ui::makeButton(content(), "Stop", layout.stopButton.x, layout.stopButton.y,
                                            layout.stopButton.w, layout.stopButton.h, app_ui::ButtonKind::Danger);
            lv_obj_add_event_cb(stop, AppAudioTool::onStop, LV_EVENT_CLICKED, this);
        }

        auto* panel = app_ui::makePanel(content(), layout.statusPanel.x, layout.statusPanel.y, layout.statusPanel.w,
                                        layout.statusPanel.h);
        auto* title = app_ui::makeLabel(panel, "Status", layout.statusTitle.x, layout.statusTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.statusTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _detail = app_ui::makeLabel(panel, "", layout.statusDetail.x, layout.statusDetail.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.statusDetail.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        refresh();
    }

    void refresh()
    {
        const auto sample = _controller.sample();
        setMetric(_state, "State", sample.state.c_str());
        if (sample.busy) lv_obj_add_state(_start, LV_STATE_DISABLED);
        else lv_obj_remove_state(_start, LV_STATE_DISABLED);
        if (_detail != nullptr) {
            lv_label_set_text_fmt(_detail, "%s\nSampling: 4 Hz while this app is open", sample.detail.c_str());
        }
    }

    void start()
    {
        _controller.start();
        refresh();
    }

    void stop()
    {
        _controller.stop();
        refresh();
    }

    void setMetric(lv_obj_t* card, const char* title, const char* value)
    {
        if (card == nullptr) {
            return;
        }
        auto* titleLabel = lv_obj_get_child(card, 0);
        if (titleLabel != nullptr) {
            lv_label_set_text(titleLabel, title);
        }
        auto* valueLabel = lv_obj_get_child(card, 1);
        if (valueLabel != nullptr) {
            lv_label_set_text(valueLabel, value);
        }
    }

    static void onStart(lv_event_t* event)
    {
        auto* app = static_cast<AppAudioTool*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->start();
        }
    }

    static void onStop(lv_event_t* event)
    {
        auto* app = static_cast<AppAudioTool*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->stop();
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_media_audio::makeDualMicApp()
{
    AudioToolCallbacks callbacks;
    callbacks.status = [] { return GetHAL()->audioGetStatus(); };
    callbacks.stop = [] { GetHAL()->audioCancel(); };
    callbacks.start = [] { GetHAL()->startDualMicRecordTest(); };
    callbacks.micState = [] { return GetHAL()->getDualMicRecordTestState(); };
    AudioToolConfig config;
    config.kind = AudioToolKind::DualMic;
    config.hasStop = true;
    config.title = "Dual Mic";
    config.description = "Record from the dual microphones and play back the result.";
    config.callbacks = std::move(callbacks);
    return std::make_unique<AppAudioTool>(std::move(config));
}

std::unique_ptr<mooncake::AppAbility> app_media_audio::makeHeadphoneApp()
{
    AudioToolCallbacks callbacks;
    callbacks.status = [] { return GetHAL()->audioGetStatus(); };
    callbacks.stop = [] { GetHAL()->audioCancel(); };
    callbacks.start = [] { GetHAL()->startHeadphoneMicRecordTest(); };
    callbacks.micState = [] { return GetHAL()->getHeadphoneMicRecordTestState(); };
    callbacks.headphoneDetected = [] { return GetHAL()->headPhoneDetect(); };
    AudioToolConfig config;
    config.kind = AudioToolKind::Headphone;
    config.hasStop = true;
    config.title = "Headphone";
    config.description = "Record from the headset microphone and play back the result.";
    config.callbacks = std::move(callbacks);
    return std::make_unique<AppAudioTool>(std::move(config));
}

std::unique_ptr<mooncake::AppAbility> app_media_audio::makeMusicApp()
{
    AudioToolCallbacks callbacks;
    callbacks.status = [] { return GetHAL()->audioGetStatus(); };
    callbacks.stop = [] { GetHAL()->audioCancel(); };
    callbacks.start = [] { GetHAL()->startPlayMusicTest(); };
    callbacks.stop = [] { GetHAL()->stopPlayMusicTest(); };
    callbacks.musicState = [] { return GetHAL()->getMusicPlayTestState(); };
    AudioToolConfig config;
    config.kind = AudioToolKind::Music;
    config.title = "Music";
    config.description = "Speaker playback test.";
    config.hasStop = true;
    config.callbacks = std::move(callbacks);
    return std::make_unique<AppAudioTool>(std::move(config));
}

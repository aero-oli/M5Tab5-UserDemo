/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_rtc.h"
#include "rtc_controller.h"
#include "rtc_layout.h"

#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_hardware_rtc;

namespace {

class AppRtc : public mooncake::AppAbility {
public:
    AppRtc()
        : _controller([] { GetHAL()->updateRtcData(); }, [] { return GetHAL()->getRtcData(); },
                      [](const tm& time, bool initialize) { return GetHAL()->setRtcTimeChecked(time, initialize); })
    {
        setAppInfo().name = "RTC";
    }

    void onOpen() override
    {
        _seeded = false;
        _editorError.clear();
        _shell = std::make_unique<app_ui::AppShell>("RTC", [&]() { close(); });
        _shell->init();
        build();
        _controller.sample();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 100) return;
        if (now - _lastPollMs >= 500) {
            _controller.sample();
            _lastPollMs = now;
        }
        LvglLockGuard lock;
        refresh();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _shell.reset();
    }

private:
    RtcController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _time        = nullptr;
    lv_obj_t* _date        = nullptr;
    lv_obj_t* _detail      = nullptr;
    uint32_t _lastUpdateMs = 0, _lastPollMs = 0;
    lv_obj_t* _dateInput = nullptr;
    lv_obj_t* _timeInput = nullptr;
    lv_obj_t* _setButton = nullptr;
    bool _seeded         = false;
    std::string _editorError;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeRtcLayout();
        auto* description = app_ui::makeLabel(
            content(), "Read and set the clock. Date: YYYY-MM-DD; time: HH:MM:SS (2000-2099).", layout.description.x,
            layout.description.y, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _time = app_ui::makeMetricCard(content(), "Current Time", "--:--:--", layout.timeCard.x, layout.timeCard.y,
                                       layout.timeCard.w, layout.timeCard.h);
        _date = app_ui::makeMetricCard(content(), "Date", "---- -- --", layout.dateCard.x, layout.dateCard.y,
                                       layout.dateCard.w, layout.dateCard.h);

        _dateInput = app_ui::makeTextArea(content(), "YYYY-MM-DD", layout.dateInput.x, layout.dateInput.y,
                                          layout.dateInput.w, layout.dateInput.h);
        _timeInput = app_ui::makeTextArea(content(), "HH:MM:SS", layout.timeInput.x, layout.timeInput.y,
                                          layout.timeInput.w, layout.timeInput.h);
        _shell->bindTextInput(_dateInput);
        _shell->bindTextInput(_timeInput);
        _setButton = app_ui::makeButton(content(), "Set clock", layout.setButton.x, layout.setButton.y,
                                        layout.setButton.w, layout.setButton.h, app_ui::ButtonKind::Primary);
        lv_obj_add_event_cb(
            _setButton, [](lv_event_t* event) { static_cast<AppRtc*>(lv_event_get_user_data(event))->requestSet(); },
            LV_EVENT_CLICKED, this);

        auto* panel = app_ui::makePanel(content(), layout.sourcePanel.x, layout.sourcePanel.y, layout.sourcePanel.w,
                                        layout.sourcePanel.h);
        auto* title = app_ui::makeLabel(panel, "RTC Status", layout.sourceTitle.x, layout.sourceTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.sourceTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _detail = app_ui::makeLabel(panel, "", layout.sourceDetail.x, layout.sourceDetail.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.sourceDetail.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        refresh();
    }

    void requestSet()
    {
        tm desired{};
        if (!RtcController::parseEditor(lv_textarea_get_text(_dateInput), lv_textarea_get_text(_timeInput), desired)) {
            _editorError = "Invalid date/time. Use YYYY-MM-DD and HH:MM:SS, years 2000-2099.";
            refresh();
            return;
        }
        _editorError.clear();
        const bool initialize   = _controller.cached().needsInitialization;
        const std::string title = initialize ? "Initialize RTC and set time?" : "Set clock?";
        std::string message =
            initialize ? "Initialize RTC and set time (clears wake configuration). Alarm and timed wake settings will "
                         "be cleared."
                       : "Set the hardware clock. Wake settings are preserved; the seconds divider phase resets.";
        message += "\n\n" + std::string(lv_textarea_get_text(_dateInput)) + " " + lv_textarea_get_text(_timeInput);
        if (_controller.cached().simulated)
            message += "\nThis desktop clock is simulated; the host clock stays unchanged.";
        _shell->confirm(title, message, [this, desired, initialize] {
            if (!_controller.setClock(desired, initialize))
                _editorError = "RTC busy or unavailable; retry after the current operation.";
            refresh();
        });
    }

    void refresh()
    {
        const auto sample = _controller.cached();
        setMetric(_time, "Current Time", RtcController::formatTime(sample).c_str());
        setMetric(_date, "Date", RtcController::formatDate(sample).c_str());
        if (!_seeded && sample.hasSample && lv_textarea_get_text(_dateInput)[0] == '\0' &&
            lv_textarea_get_text(_timeInput)[0] == '\0') {
            lv_textarea_set_text(_dateInput, RtcController::formatDate(sample).c_str());
            lv_textarea_set_text(_timeInput, RtcController::formatTime(sample).c_str());
            _seeded = true;
        }
        if (sample.pending)
            lv_obj_add_state(_setButton, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(_setButton, LV_STATE_DISABLED);
        std::string detail = RtcController::statusText(sample, GetHAL()->millis());
        if (sample.hasSample) detail += " | " + RtcController::formatWeekday(sample);
        if (sample.pending) detail += " | Working...";
        if (!sample.commandMessage.empty()) detail += "\n" + sample.commandMessage;
        if (!_editorError.empty()) detail += "\n" + _editorError;
        if (sample.hasSample && !sample.valid) detail += "\nDisplayed values are the last successful sample.";
        if (sample.needsInitialization)
            detail += "\nSetting the clock requires confirmed initialization and clears wake configuration.";
        lv_label_set_text(_detail, detail.c_str());
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
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_rtc::makeRtcApp()
{
    return std::make_unique<AppRtc>();
}

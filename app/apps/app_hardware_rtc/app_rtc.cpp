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
    AppRtc() : _controller([](tm* time) { GetHAL()->getRtcTime(time); })
    {
        setAppInfo().name = "RTC";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("RTC", [&]() { close(); });
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
        _shell.reset();
    }

private:
    RtcController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _time = nullptr;
    lv_obj_t* _date = nullptr;
    lv_obj_t* _detail = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeRtcLayout();
        auto* description = app_ui::makeLabel(content(), "Live hardware clock readout.", layout.description.x,
                                              layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _time = app_ui::makeMetricCard(content(), "Current Time", "--:--:--", layout.timeCard.x, layout.timeCard.y,
                                       layout.timeCard.w, layout.timeCard.h);
        _date = app_ui::makeMetricCard(content(), "Date", "---- -- --", layout.dateCard.x, layout.dateCard.y,
                                       layout.dateCard.w, layout.dateCard.h);

        auto* panel = app_ui::makePanel(content(), layout.sourcePanel.x, layout.sourcePanel.y, layout.sourcePanel.w,
                                        layout.sourcePanel.h);
        auto* title = app_ui::makeLabel(panel, "RTC Source", layout.sourceTitle.x, layout.sourceTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.sourceTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _detail = app_ui::makeLabel(panel, "", layout.sourceDetail.x, layout.sourceDetail.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.sourceDetail.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        refresh();
    }

    void refresh()
    {
        const auto sample = _controller.sample();
        setMetric(_time, "Current Time", RtcController::formatTime(sample).c_str());
        setMetric(_date, "Date", RtcController::formatDate(sample).c_str());
        if (_detail != nullptr) {
            lv_label_set_text_fmt(_detail, "%s\nSampling: 4 Hz while this app is open",
                                  RtcController::formatWeekday(sample).c_str());
        }
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

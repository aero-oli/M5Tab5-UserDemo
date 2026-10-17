/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_diagnostics.h"
#include "diagnostics_layout.h"
#include <cstdio>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/runtime/performance_telemetry.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_diagnostics;

namespace {

class AppDiagnostics : public mooncake::AppAbility {
public:
    AppDiagnostics()
    {
        setAppInfo().name = "Diagnostics";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Diagnostics", [&]() { close(); });
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
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _loop = nullptr;
    lv_obj_t* _slow = nullptr;
    lv_obj_t* _lvgl = nullptr;
    lv_obj_t* _task = nullptr;
    lv_obj_t* _detail = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeDiagnosticsLayout();
        auto* description = app_ui::makeLabel(content(), "Runtime counters for UI responsiveness and background jobs.",
                                              layout.description.x, layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        _loop = makeMetric(layout, 0, "App Loop");
        _slow = makeMetric(layout, 1, "Slow Updates");
        _lvgl = makeMetric(layout, 2, "LVGL");
        _task = makeMetric(layout, 3, "Background Task");
        auto* panel = app_ui::makePanel(content(), layout.detailPanel.x, layout.detailPanel.y, layout.detailPanel.w,
                                        layout.detailPanel.h);
        auto* hint = app_ui::makeLabel(
            panel,
            "Use this screen when the UI feels sluggish. Slow updates count app-loop samples over 50 ms; the average is a rolling window.",
            layout.detailHint.x, layout.detailHint.y, &lv_font_montserrat_18,
            app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(hint, layout.detailHint.w);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        _detail = app_ui::makeLabel(panel, "", layout.detailText.x, layout.detailText.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(_detail, layout.detailText.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        refresh();
    }

    lv_obj_t* makeMetric(const DiagnosticsLayout& layout, int index, const char* title)
    {
        const auto rect = diagnosticsMetricRect(layout, index);
        return app_ui::makeMetricCard(content(), title, "--", rect.x, rect.y, rect.w, rect.h);
    }

    void refresh()
    {
        auto perf = app_runtime::globalPerformanceTelemetry().snapshot();
        setMetric(_loop, "App Loop", "%ums avg %ums", static_cast<unsigned>(perf.lastAppUpdateMs),
                  static_cast<unsigned>(perf.avgAppUpdateMs));
        setMetric(_slow, "Slow Updates", "%u", static_cast<unsigned>(perf.slowAppUpdates));
        setMetric(_lvgl, "LVGL", "%ums max %ums", static_cast<unsigned>(perf.lastLvglMs),
                  static_cast<unsigned>(perf.maxLvglMs));
        setMetric(_task, "Background Task", "%s",
                  perf.backgroundBusy ? (perf.backgroundJob.empty() ? "Busy" : perf.backgroundJob.c_str()) : "Idle");
        if (_detail != nullptr) {
            lv_label_set_text_fmt(_detail, "Updates: %u\nMax app loop: %ums\nCurrent task: %s",
                                  static_cast<unsigned>(perf.totalAppUpdates), static_cast<unsigned>(perf.maxAppUpdateMs),
                                  perf.backgroundBusy ? (perf.backgroundJob.empty() ? "busy" : perf.backgroundJob.c_str()) : "none");
        }
    }

    template <typename... Args>
    void setMetric(lv_obj_t* card, const char* title, const char* format, Args... args)
    {
        if (card == nullptr) {
            return;
        }
        char value[96];
        std::snprintf(value, sizeof(value), format, args...);
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

std::unique_ptr<mooncake::AppAbility> app_diagnostics::makeDiagnosticsApp()
{
    return std::make_unique<AppDiagnostics>();
}

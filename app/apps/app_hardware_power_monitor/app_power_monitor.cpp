/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_power_monitor.h"
#include "power_monitor_controller.h"
#include "power_monitor_layout.h"

#include <cstdio>
#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_hardware_power_monitor;

namespace {

class AppPowerMonitor : public mooncake::AppAbility {
public:
    AppPowerMonitor()
        : _controller([] { GetHAL()->updatePowerMonitorData(); }, [] { return GetHAL()->getPowerMonitorData(); },
                      [](int& value) { return GetHAL()->readCpuTemp(value); })
    {
        setAppInfo().name = "Power Monitor";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Power Monitor", [&]() { close(); });
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
    PowerMonitorController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _voltage     = nullptr;
    lv_obj_t* _current     = nullptr;
    lv_obj_t* _power       = nullptr;
    lv_obj_t* _cpu         = nullptr;
    lv_obj_t* _detail      = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makePowerMonitorLayout();
        auto* description = app_ui::makeLabel(content(), "Live input, rail and CPU telemetry from the power monitor.",
                                              layout.description.x, layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _voltage = makeMetric(layout, 0, "Bus Voltage");
        _current = makeMetric(layout, 1, "Current");
        _power   = makeMetric(layout, 2, "Power");
        _cpu     = makeMetric(layout, 3, "CPU Temp");

        auto* panel = app_ui::makePanel(content(), layout.readoutPanel.x, layout.readoutPanel.y, layout.readoutPanel.w,
                                        layout.readoutPanel.h);
        auto* title = app_ui::makeLabel(panel, "Readout", layout.readoutTitle.x, layout.readoutTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.readoutTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _detail = app_ui::makeLabel(panel, "", layout.readoutDetail.x, layout.readoutDetail.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.readoutDetail.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
        refresh();
    }

    lv_obj_t* makeMetric(const PowerMonitorLayout& layout, int index, const char* title)
    {
        const auto rect = powerMonitorMetricRect(layout, index);
        return app_ui::makeMetricCard(content(), title, "--", rect.x, rect.y, rect.w, rect.h);
    }

    void refresh()
    {
        const auto sample = _controller.sample();
        setMetric(_voltage, "Bus Voltage", PowerMonitorController::formatVoltage(sample).c_str());
        setMetric(_current, "Current", PowerMonitorController::formatCurrent(sample).c_str());
        setMetric(_power, "Power", PowerMonitorController::formatPower(sample).c_str());
        setMetric(_cpu, "CPU Temp", PowerMonitorController::formatCpuTemp(sample).c_str());
        if (_detail != nullptr) {
            const auto status = PowerMonitorController::statusText(sample, GetHAL()->millis());
            char shunt[48]    = {};
            if (sample.hasSample) std::snprintf(shunt, sizeof(shunt), "Shunt: %.3f V\n", sample.shuntVoltage);
            const auto detail =
                std::string(shunt) + status + (sample.cpuTempValid ? "" : "\nCPU temperature unavailable");
            lv_label_set_text(_detail, detail.c_str());
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

std::unique_ptr<mooncake::AppAbility> app_hardware_power_monitor::makePowerMonitorApp()
{
    return std::make_unique<AppPowerMonitor>();
}

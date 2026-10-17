/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_i2c_scan.h"
#include "i2c_scan_controller.h"
#include "i2c_scan_layout.h"
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_hardware_i2c;

namespace {

enum class ActionId {
    ScanPortA,
    ScanInternal,
};

class AppI2cScan;

struct ActionBinding {
    AppI2cScan* app = nullptr;
    ActionId id     = ActionId::ScanPortA;
};

class AppI2cScan : public mooncake::AppAbility {
public:
    AppI2cScan() : _controller([](bool internalBus) { return GetHAL()->i2cScan(internalBus); })
    {
        setAppInfo().name = "I2C Scan";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("I2C Scan", [&]() { close(); });
        _shell->init();
        build();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 100) {
            return;
        }
        _controller.refresh();
        LvglLockGuard lock;
        refreshView();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _actions.clear();
        _shell.reset();
    }

private:
    I2cScanController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<ActionBinding>> _actions;
    lv_obj_t* _status      = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeI2cScanLayout();
        auto* description = app_ui::makeLabel(
            content(), "Scan M-Bus, Port A, and onboard I2C devices without blocking the UI.", layout.description.x,
            layout.description.y, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        addButton(layout, 0, "Scan Port A", app_ui::ButtonKind::Primary, ActionId::ScanPortA);
        addButton(layout, 1, "Scan Internal", app_ui::ButtonKind::Secondary, ActionId::ScanInternal);

        auto* panel = app_ui::makePanel(content(), layout.resultsPanel.x, layout.resultsPanel.y, layout.resultsPanel.w,
                                        layout.resultsPanel.h);
        auto* title = app_ui::makeLabel(panel, "Results", layout.resultsTitle.x, layout.resultsTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.resultsTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _status = app_ui::makeLabel(panel, _controller.statusText().c_str(), layout.resultsText.x, layout.resultsText.y,
                                    &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(_status, layout.resultsText.w);
        lv_label_set_long_mode(_status, LV_LABEL_LONG_WRAP);
    }

    void addButton(const I2cScanLayout& layout, int index, const char* text, app_ui::ButtonKind kind, ActionId id)
    {
        auto binding    = std::make_unique<ActionBinding>();
        binding->app    = this;
        binding->id     = id;
        const auto rect = i2cScanActionRect(layout, index);
        auto* button    = app_ui::makeButton(content(), text, rect.x, rect.y, rect.w, rect.h, kind);
        lv_obj_add_event_cb(button, AppI2cScan::onAction, LV_EVENT_CLICKED, binding.get());
        _actions.push_back(std::move(binding));
    }

    void handle(ActionId id)
    {
        audio::play_next_tone_progression();
        _controller.startScan(id == ActionId::ScanInternal);
        refreshView();
    }

    void refreshView()
    {
        if (_status != nullptr) {
            lv_label_set_text(_status, _controller.statusText().c_str());
            lv_obj_set_style_text_color(_status,
                                        _controller.busy() ? app_ui::theme::color(app_ui::theme::amber)
                                                           : app_ui::theme::color(app_ui::theme::text),
                                        0);
        }
    }

    static void onAction(lv_event_t* event)
    {
        auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
        if (binding != nullptr && binding->app != nullptr) {
            binding->app->handle(binding->id);
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_i2c::makeI2cScanApp()
{
    return std::make_unique<AppI2cScan>();
}

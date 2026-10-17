/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_sd_card.h"
#include "sd_card_controller.h"
#include "sd_card_layout.h"
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_hardware_sd;

namespace {

class AppSdCard : public mooncake::AppAbility {
public:
    AppSdCard() : _controller([](const std::string& path) { return GetHAL()->scanSdCardResult(path); })
    {
        setAppInfo().name = "SD Card";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("SD Card", [&]() { close(); });
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
        _shell.reset();
        _status = _scanButton = _results = nullptr;
    }

private:
    SdCardController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _status     = nullptr;
    lv_obj_t* _scanButton = nullptr;
    lv_obj_t* _results    = nullptr;
    std::string _renderedText;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeSdCardLayout();

        auto* description = app_ui::makeLabel(content(), "Mount and scan the SD card root without blocking navigation.",
                                              layout.description.x, layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _scanButton = app_ui::makeButton(content(), "Scan SD Card", layout.scanButton.x, layout.scanButton.y,
                                         layout.scanButton.w, layout.scanButton.h, app_ui::ButtonKind::Primary);
        lv_obj_add_event_cb(_scanButton, AppSdCard::onScan, LV_EVENT_CLICKED, this);

        auto* panel = app_ui::makePanel(content(), layout.resultsPanel.x, layout.resultsPanel.y, layout.resultsPanel.w,
                                        layout.resultsPanel.h);
        auto* title = app_ui::makeLabel(panel, "Root", layout.resultsTitle.x, layout.resultsTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.resultsTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _results      = app_ui::makeScrollableContent(panel, layout.resultsText.x, layout.resultsText.y,
                                                      layout.resultsText.w, layout.resultsText.h);
        _renderedText = _controller.statusText();
        _status       = app_ui::makeLabel(_results, _renderedText.c_str(), 0, 0, &lv_font_montserrat_18,
                                          app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(_status, layout.resultsText.w - 20);
        lv_label_set_long_mode(_status, LV_LABEL_LONG_WRAP);
        refreshView();
    }

    void startScan()
    {
        audio::play_next_tone_progression();
        _controller.startScan();
        refreshView();
    }

    void refreshView()
    {
        const bool busy = _controller.busy();
        if (_scanButton) {
            if (busy)
                lv_obj_add_state(_scanButton, LV_STATE_DISABLED);
            else
                lv_obj_remove_state(_scanButton, LV_STATE_DISABLED);
        }
        if (_status) {
            const auto text = _controller.statusText();
            if (text != _renderedText) {
                _renderedText = text;
                lv_label_set_text(_status, text.c_str());
                lv_obj_scroll_to_y(_results, 0, LV_ANIM_OFF);
            }
            lv_obj_set_style_text_color(
                _status, busy ? app_ui::theme::color(app_ui::theme::amber) : app_ui::theme::color(app_ui::theme::text),
                0);
        }
    }

    static void onScan(lv_event_t* event)
    {
        auto* app = static_cast<AppSdCard*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->startScan();
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_sd::makeSdCardApp()
{
    return std::make_unique<AppSdCard>();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_com_monitor.h"
#include "com_monitor_controller.h"
#include "com_monitor_layout.h"

#include <memory>
#include <string>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_hardware_com_monitor;

namespace {

class AppComMonitor : public mooncake::AppAbility {
public:
    AppComMonitor()
        : _controller([]() -> hal::HalBase::UartMonitorData_t& { return GetHAL()->uartMonitorData; },
                      [](const std::string& msg, bool newLine) { GetHAL()->uartMonitorSend(msg, newLine); })
    {
        setAppInfo().name = "COM Monitor";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("COM Monitor", [&]() { close(); });
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
    ComMonitorController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _log = nullptr;
    lv_obj_t* _status = nullptr;
    std::string _logText;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeComMonitorLayout();
        auto* description = app_ui::makeLabel(content(), "RS485 serial monitor.", layout.description.x,
                                              layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);

        auto* panel = app_ui::makePanel(content(), layout.logPanel.x, layout.logPanel.y, layout.logPanel.w,
                                        layout.logPanel.h);
        auto* title = app_ui::makeLabel(panel, "RX Log", layout.logTitle.x, layout.logTitle.y, &lv_font_montserrat_20,
                                        app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.logTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _log = app_ui::makeLabel(panel, "No data received.", layout.logText.x, layout.logText.y, &lv_font_montserrat_18,
                                 app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_log, layout.logText.w);
        lv_label_set_long_mode(_log, LV_LABEL_LONG_WRAP);

        auto* send = app_ui::makeButton(content(), "Send Test Message", layout.sendButton.x, layout.sendButton.y,
                                        layout.sendButton.w, layout.sendButton.h, app_ui::ButtonKind::Primary);
        lv_obj_add_event_cb(send, AppComMonitor::onSend, LV_EVENT_CLICKED, this);
        _status = app_ui::makeLabel(content(), "Polling at 4 Hz while this app is open.", layout.statusText.x,
                                    layout.statusText.y, &lv_font_montserrat_16,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_status, layout.statusText.w);
        lv_label_set_long_mode(_status, LV_LABEL_LONG_DOT);
    }

    void refresh()
    {
        auto sample = _controller.drainRx();
        if (sample.text.empty()) {
            return;
        }
        _logText += sample.text;
        if (_logText.size() > 2000) {
            _logText.erase(0, _logText.size() - 2000);
        }
        if (_log != nullptr) {
            lv_label_set_text(_log, _logText.c_str());
        }
        if (_status != nullptr) {
            lv_label_set_text_fmt(_status, "Received %u bytes this poll. Showing last 2000 bytes.",
                                  static_cast<unsigned>(sample.bytesRead));
        }
    }

    void sendTestMessage()
    {
        audio::play_next_tone_progression();
        _controller.sendTestMessage();
        if (_status != nullptr) {
            lv_label_set_text(_status, "Sent test message with newline.");
        }
    }

    static void onSend(lv_event_t* event)
    {
        auto* app = static_cast<AppComMonitor*>(lv_event_get_user_data(event));
        if (app != nullptr) {
            app->sendTestMessage();
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_com_monitor::makeComMonitorApp()
{
    return std::make_unique<AppComMonitor>();
}

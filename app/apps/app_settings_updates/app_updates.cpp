/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_updates.h"
#include "updates_controller.h"
#include "updates_layout.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_settings_updates;

namespace {

enum class ActionId {
    StartOta,
    CancelOta,
    RebootOta,
};

class AppUpdates;

struct ActionBinding {
    AppUpdates* app = nullptr;
    ActionId id = ActionId::StartOta;
};

class AppUpdates : public mooncake::AppAbility {
public:
    AppUpdates() : _controller(makeCallbacks())
    {
        setAppInfo().name = "Updates";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Updates", [&]() { close(); });
        _shell->init();
        _shell->setContentScrollable(true);
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
        _urlDraft = lv_textarea_get_text(_url);
        _actions.clear();
        _shell.reset();
    }

private:
    UpdatesController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<ActionBinding>> _actions;
    uint32_t _lastUpdateMs = 0;
    std::string _urlDraft;

    lv_obj_t* _primary = nullptr;
    lv_obj_t* _secondary = nullptr;
    lv_obj_t* _detail = nullptr;
    lv_obj_t* _url = nullptr;
    lv_obj_t* _progress = nullptr;
    lv_obj_t* _progressText = nullptr;
    lv_obj_t* _installButton = nullptr;
    lv_obj_t* _cancelButton = nullptr;
    lv_obj_t* _rebootButton = nullptr;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    lv_obj_t* bindButton(const char* text, int x, int y, int w, int h, app_ui::ButtonKind kind, ActionId id)
    {
        auto binding = std::make_unique<ActionBinding>();
        binding->app = this;
        binding->id = id;
        auto* button = app_ui::makeButton(content(), text, x, y, w, h, kind);
        lv_obj_add_event_cb(button, AppUpdates::onAction, LV_EVENT_CLICKED, binding.get());
        _actions.push_back(std::move(binding));
        return button;
    }

    void build()
    {
        LvglLockGuard lock;
        auto layout = makeUpdatesLayout();
        _primary = app_ui::makeMetricCard(content(), "Firmware", "M5Tab5", layout.firmwareCard.x, layout.firmwareCard.y, layout.firmwareCard.w, layout.firmwareCard.h);
        _secondary = app_ui::makeMetricCard(content(), "Update State", "--", layout.stateCard.x, layout.stateCard.y, layout.stateCard.w, layout.stateCard.h);
        auto* panel = app_ui::makePanel(content(), layout.otaPanel.x, layout.otaPanel.y, layout.otaPanel.w, layout.otaPanel.h);
        app_ui::makeWrappedLabel(panel, "HTTPS firmware update", 0, 0, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        app_ui::makeWrappedLabel(
            panel,
            "Use an HTTPS .bin URL from a source you trust. Reboot after a verified install.",
            0,
            36,
            &lv_font_montserrat_16,
            app_ui::theme::color(app_ui::theme::muted));
        _url = app_ui::makeTextArea(panel, "https://example.com/m5stack_tab5.bin", layout.urlInput.x, layout.urlInput.y, layout.urlInput.w, layout.urlInput.h);
        lv_textarea_set_max_length(_url, 512);
        lv_textarea_set_text(_url, _urlDraft.c_str());
        _shell->bindTextInput(_url);
        _installButton =
            bindButton("Install Update", layout.installButton.x, layout.installButton.y, layout.installButton.w, layout.installButton.h, app_ui::ButtonKind::Primary, ActionId::StartOta);
        _cancelButton = bindButton("Cancel Update", layout.cancelButton.x, layout.cancelButton.y, layout.cancelButton.w, layout.cancelButton.h, app_ui::ButtonKind::Secondary, ActionId::CancelOta);
        _rebootButton = bindButton("Reboot", layout.rebootButton.x, layout.rebootButton.y, layout.rebootButton.w, layout.rebootButton.h, app_ui::ButtonKind::Danger, ActionId::RebootOta);
        _progress = app_ui::makeProgressBar(content(), layout.progressBar.x, layout.progressBar.y, layout.progressBar.w, layout.progressBar.h, 0, 100);
        _progressText = app_ui::makeFittedLabel(content(), "", 0, 440, 920, 24, &lv_font_montserrat_16,
            app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        _detail = app_ui::makeWrappedLabel(content(),
                                           "",
                                           layout.detailText.x,
                                           layout.detailText.y,
                                           &lv_font_montserrat_18,
                                           app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.detailText.w);
        lv_obj_set_height(_detail, layout.detailText.h);
        refresh();
    }

    void refresh()
    {
        const auto snapshot = _controller.refresh();
        refreshMetric(_primary, "Target", "%s", snapshot.ota.simulated ? "Desktop simulator" : "M5Tab5");
        refreshMetric(_secondary, "Update State", "%s", snapshot.stateText.c_str());
        if (_progress != nullptr) {
            lv_bar_set_value(_progress, snapshot.ota.progress, LV_ANIM_OFF);
            if (snapshot.ota.progressKnown) lv_obj_remove_flag(_progress, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(_progress, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(_progressText, snapshot.progressText.c_str());
        if (_detail != nullptr) {
            lv_label_set_text(_detail, snapshot.detail.c_str());
        }
        setEnabled(_installButton, snapshot.canInstall);
        setEnabled(_cancelButton, snapshot.canCancel);
        setEnabled(_rebootButton, snapshot.canReboot);
        setEnabled(_url, !snapshot.busy && !snapshot.ota.rebootRequired);
    }

    static void setEnabled(lv_obj_t* object, bool enabled)
    {
        if (enabled) lv_obj_remove_state(object, LV_STATE_DISABLED);
        else lv_obj_add_state(object, LV_STATE_DISABLED);
    }

    template <typename... Args>
    void refreshMetric(lv_obj_t* card, const char* title, const char* fmt, Args... args)
    {
        if (card == nullptr) {
            return;
        }
        char value[96];
        std::snprintf(value, sizeof(value), fmt, args...);
        auto* valueLabel = lv_obj_get_child(card, 1);
        if (valueLabel != nullptr) {
            lv_label_set_text(valueLabel, value);
        }
        auto* titleLabel = lv_obj_get_child(card, 0);
        if (titleLabel != nullptr) {
            lv_label_set_text(titleLabel, title);
        }
    }

    void handle(ActionId id)
    {
        switch (id) {
            case ActionId::StartOta: {
                const std::string url = lv_textarea_get_text(_url);
                if (!_controller.prepareInstall(url)) break;
                _shell->confirm("Install firmware update?",
                    "Only install firmware from a source you trust. Keep device power connected.\n\n" + url,
                    [this, url] { _controller.start(url); refresh(); });
                break;
            }
            case ActionId::CancelOta:
                _controller.cancel();
                break;
            case ActionId::RebootOta:
                if (_controller.refresh().canReboot) {
                    _shell->confirm("Reboot to apply update?",
                        "The device will restart. Disconnect connected loads first; external module outputs may remain powered.",
                        [this] { _controller.rebootToApply(); refresh(); });
                }
                break;
        }
        refresh();
    }

    static UpdatesCallbacks makeCallbacks()
    {
        UpdatesCallbacks callbacks;
        callbacks.wifiStatus = [] { return GetHAL()->wifiGetStatus(); };
        callbacks.otaStatus = [] { return GetHAL()->otaGetStatus(); };
        callbacks.start = [](const std::string& url) { return GetHAL()->otaStartFromUrl(url); };
        callbacks.cancel = [] { GetHAL()->otaCancel(); };
        callbacks.reboot = [] { GetHAL()->otaRebootToApply(); };
        return callbacks;
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

std::unique_ptr<mooncake::AppAbility> app_settings_updates::makeUpdatesApp()
{
    return std::make_unique<AppUpdates>();
}

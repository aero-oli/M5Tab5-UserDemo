/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_display_sound.h"
#include "display_sound_controller.h"
#include "display_sound_layout.h"

#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_settings_display_sound;

namespace {

enum class ActionId {
    BrightnessDown,
    BrightnessUp,
    VolumeDown,
    VolumeUp,
};

struct ActionBinding {
    class AppDisplaySound* app = nullptr;
    ActionId id = ActionId::BrightnessDown;
};

class AppDisplaySound : public mooncake::AppAbility {
public:
    AppDisplaySound()
        : _controller([] { return GetHAL()->getDisplayBrightness(); },
                      [] { return GetHAL()->getSpeakerVolume(); },
                      [](uint8_t value) { GetHAL()->setDisplayBrightness(value); },
                      [](uint8_t value) { GetHAL()->setSpeakerVolume(value); })
    {
        setAppInfo().name = "Display & Sound";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Display & Sound", [&]() { close(); });
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
        _actions.clear();
        _shell.reset();
    }

private:
    DisplaySoundController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<ActionBinding>> _actions;
    lv_obj_t* _brightness = nullptr;
    lv_obj_t* _volume = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeDisplaySoundLayout();
        auto* description = app_ui::makeLabel(content(), "Device preference controls for screen brightness and speaker volume.",
                                              layout.description.x, layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _brightness = app_ui::makeMetricCard(content(), "Display Brightness", "--", layout.brightnessCard.x,
                                             layout.brightnessCard.y, layout.brightnessCard.w,
                                             layout.brightnessCard.h);
        _volume = app_ui::makeMetricCard(content(), "Speaker Volume", "--", layout.volumeCard.x,
                                         layout.volumeCard.y, layout.volumeCard.w, layout.volumeCard.h);
        bindButton(layout, 0, "Brightness -", app_ui::ButtonKind::Secondary, ActionId::BrightnessDown);
        bindButton(layout, 1, "Brightness +", app_ui::ButtonKind::Primary, ActionId::BrightnessUp);
        bindButton(layout, 2, "Volume -", app_ui::ButtonKind::Secondary, ActionId::VolumeDown);
        bindButton(layout, 3, "Volume +", app_ui::ButtonKind::Primary, ActionId::VolumeUp);

        auto* panel = app_ui::makePanel(content(), layout.infoPanel.x, layout.infoPanel.y, layout.infoPanel.w,
                                        layout.infoPanel.h);
        auto* info = app_ui::makeLabel(panel, "Changes apply immediately and are clamped to 0-100%.",
                                       layout.infoText.x, layout.infoText.y, &lv_font_montserrat_18,
                                       app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(info, layout.infoText.w);
        lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
        refresh();
    }

    void bindButton(const DisplaySoundLayout& layout, int index, const char* text, app_ui::ButtonKind kind, ActionId id)
    {
        auto binding = std::make_unique<ActionBinding>();
        binding->app = this;
        binding->id = id;
        const auto rect = displaySoundActionRect(layout, index);
        auto* button = app_ui::makeButton(content(), text, rect.x, rect.y, rect.w, rect.h, kind);
        lv_obj_add_event_cb(button, AppDisplaySound::onAction, LV_EVENT_CLICKED, binding.get());
        _actions.push_back(std::move(binding));
    }

    void handle(ActionId id)
    {
        audio::play_next_tone_progression();
        switch (id) {
            case ActionId::BrightnessDown:
                _controller.adjustBrightness(-10);
                break;
            case ActionId::BrightnessUp:
                _controller.adjustBrightness(10);
                break;
            case ActionId::VolumeDown:
                _controller.adjustVolume(-10);
                break;
            case ActionId::VolumeUp:
                _controller.adjustVolume(10);
                break;
        }
        refresh();
    }

    void refresh()
    {
        const auto snapshot = _controller.read();
        setMetric(_brightness, "Display Brightness", DisplaySoundController::formatPercent(snapshot.brightness).c_str());
        setMetric(_volume, "Speaker Volume", DisplaySoundController::formatPercent(snapshot.volume).c_str());
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

    static void onAction(lv_event_t* event)
    {
        auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
        if (binding != nullptr && binding->app != nullptr) {
            binding->app->handle(binding->id);
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_settings_display_sound::makeDisplaySoundApp()
{
    return std::make_unique<AppDisplaySound>();
}

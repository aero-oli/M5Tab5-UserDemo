#include "app_power_settings.h"
#include "power_settings_controller.h"
#include "power_settings_layout.h"
#include <array>
#include <cstring>
#include <memory>
#include <lvgl.h>
#include <mooncake.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_settings_power;
namespace {
class AppPowerSettings;
struct ActionBinding { AppPowerSettings* app; PowerAction action; };

class AppPowerSettings : public mooncake::AppAbility {
public:
    AppPowerSettings() { setAppInfo().name = "Power Settings"; }
    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Power", [this] { close(); });
        _shell->init();
        build();
        _controller.update(GetHAL()->millis());
    }
    void onRunning() override
    {
        _controller.update(GetHAL()->millis());
        LvglLockGuard lock;
        refresh();
    }
    void onClose() override
    {
        _controller.cancel();
        _shell.reset();
    }
private:
    PowerSettingsController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::array<ActionBinding, 4> _bindings{};
    std::array<lv_obj_t*, 4> _buttons{};
    lv_obj_t* _battery = nullptr;
    lv_obj_t* _status = nullptr;
    lv_obj_t* _message = nullptr;
    lv_obj_t* _interval = nullptr;
    lv_obj_t* _slider = nullptr;
    lv_obj_t* _wake = nullptr;
    uint16_t _minutes = 5;
    PowerState _styledState = PowerState::Ready;
    bool _styled = false;
    bool _styledAsleep = false;

    static void setText(lv_obj_t* label, const std::string& text)
    {
        if (text != lv_label_get_text(label)) lv_label_set_text(label, text.c_str());
    }
    static lv_obj_t* fitted(lv_obj_t* parent, const char* text, const app_ui::Rect& r, bool muted = false)
    {
        return app_ui::makeFittedLabel(parent, text, r.x, r.y, r.w, r.h, &lv_font_montserrat_20,
            app_ui::theme::color(muted ? app_ui::theme::muted : app_ui::theme::text),
            LV_LABEL_LONG_WRAP, LV_TEXT_ALIGN_LEFT);
    }
    void build()
    {
        LvglLockGuard lock;
        _styled = false;
        const auto l = makePowerSettingsLayout();
        auto* content = _shell->content();
        _battery = app_ui::makeMetricCard(content, "Input", "--", l.batteryCard.x, l.batteryCard.y, l.batteryCard.w, l.batteryCard.h);
        fitted(content, "Status", l.statusTitle, true);
        _status = fitted(content, "", l.statusText);
        _message = fitted(content, "", l.detailText, true);
        const char* titles[] = {"Display", "Motion Wake", "Timed Wake", "Shutdown"};
        const PowerAction actions[] = {PowerAction::SleepTouch, PowerAction::SleepShake, PowerAction::SleepRtc, PowerAction::PowerOff};
        for (int i = 0; i < 4; ++i) {
            fitted(content, titles[i], powerSettingsTitleRect(i));
            const auto r = powerSettingsActionRect(l, i);
            _buttons[i] = app_ui::makeButton(content, PowerSettingsController::actionLabel(actions[i]).c_str(),
                r.x, r.y, r.w, r.h, i == 3 ? app_ui::ButtonKind::Danger : app_ui::ButtonKind::Secondary);
            _bindings[i] = {this, actions[i]};
            lv_obj_add_event_cb(_buttons[i], [](lv_event_t* event) {
                auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
                binding->app->request(binding->action);
            }, LV_EVENT_CLICKED, &_bindings[i]);
        }
        fitted(content, "Touch wake", l.touchCaption, true);
        fitted(content, "BMI270", l.motionCaption, true);
        _interval = fitted(content, "", l.intervalLabel, true);
        _slider = app_ui::makeSlider(content, l.intervalSlider.x, l.intervalSlider.y, l.intervalSlider.w, l.intervalSlider.h, 1, 60, _minutes);
        lv_obj_add_event_cb(_slider, [](lv_event_t* event) {
            auto* app = static_cast<AppPowerSettings*>(lv_event_get_user_data(event));
            app->_minutes = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(event)));
            app->refresh();
        }, LV_EVENT_VALUE_CHANGED, this);
        fitted(content, "External outputs may remain live", l.dangerCaption, true);
        _wake = app_ui::makeButton(content, "Wake Display", l.wakeButton.x, l.wakeButton.y, l.wakeButton.w, l.wakeButton.h,
                                   app_ui::ButtonKind::Secondary);
        lv_obj_add_event_cb(_wake, [](lv_event_t* event) {
            auto* app = static_cast<AppPowerSettings*>(lv_event_get_user_data(event));
            app->_controller.wakeDisplay();
            app->refresh();
        }, LV_EVENT_CLICKED, this);
        refresh();
    }
    void request(PowerAction action)
    {
        if (!_controller.request(action, _minutes)) return;
        const auto state = _controller.state();
        auto message = PowerSettingsController::confirmation(state);
        if (action != PowerAction::SleepTouch)
            message += "\n\nPPS and other external outputs may remain live after shutdown.";
        _shell->confirm(PowerSettingsController::actionLabel(action) + "?", message,
            [this] { _controller.confirm(); refresh(); }, [this] { _controller.cancel(); refresh(); });
        refresh();
    }
    void refresh()
    {
        const auto state = _controller.state();
        setText(lv_obj_get_child(_battery, 1), state.hasSample ? PowerSettingsController::formatBatteryInput(state.sample) +
                (!state.sampleValid || GetHAL()->millis() - state.sampledAtMs > 2000 ? " (stale)" : state.sampleSimulated ? " (simulated)" : "") : "Unavailable");
        const char* status = "Ready";
        switch (state.state) {
            case PowerState::Ready: break;
            case PowerState::Confirming: status = "Confirmation"; break;
            case PowerState::Preparing: status = "Preparing"; break;
            case PowerState::DisplayAsleep: status = "Display Off"; break;
            case PowerState::Complete: status = state.simulated ? "Simulation" : "Complete"; break;
            case PowerState::Failed: status = "Failed"; break;
        }
        setText(_status, status);
        setText(_message, state.message == "Ready" ? "" : state.message);
        setText(_interval, "Wake after " + std::to_string(_minutes) + (_minutes == 1 ? " minute" : " minutes"));
        if (_styled && _styledState == state.state && _styledAsleep == state.displayAsleep) return;
        _styled = true;
        _styledState = state.state;
        _styledAsleep = state.displayAsleep;
        lv_obj_set_style_text_color(_message, app_ui::theme::color(state.state == PowerState::Failed ? app_ui::theme::red : app_ui::theme::muted), 0);
        const bool blocked = state.state == PowerState::Preparing || state.state == PowerState::Confirming ||
                             state.displayAsleep;
        for (auto* button : _buttons) {
            if (blocked) lv_obj_add_state(button, LV_STATE_DISABLED);
            else lv_obj_remove_state(button, LV_STATE_DISABLED);
        }
        if (blocked) lv_obj_add_state(_slider, LV_STATE_DISABLED);
        else lv_obj_remove_state(_slider, LV_STATE_DISABLED);
        if (state.displayAsleep && state.state != PowerState::Preparing) lv_obj_remove_flag(_wake, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_wake, LV_OBJ_FLAG_HIDDEN);
    }
};
}

std::unique_ptr<mooncake::AppAbility> app_settings_power::makePowerSettingsApp()
{
    return std::make_unique<AppPowerSettings>();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_switches.h"
#include "switches_controller.h"
#include "switches_layout.h"

#include <memory>
#include <vector>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include <apps/utils/audio/audio.h>

using namespace app_hardware_switches;

namespace {

struct ToggleBinding {
    class AppSwitches* app = nullptr;
    SwitchId id            = SwitchId::Charge;
};

class AppSwitches : public mooncake::AppAbility {
public:
    AppSwitches()
        : _controller(
              [] {
                  SwitchSnapshot snapshot;
                  snapshot.charge     = GetHAL()->getChargeEnable();
                  snapshot.chargeQc   = GetHAL()->getChargeQcEnable();
                  snapshot.ext5v      = GetHAL()->getExt5vEnable();
                  snapshot.usbA5v     = GetHAL()->getUsb5vEnable();
                  snapshot.extAntenna = GetHAL()->getExtAntennaEnable();
                  return snapshot;
              },
              [](SwitchId id, bool enabled) {
                  switch (id) {
                      case SwitchId::Charge:
                          GetHAL()->setChargeEnable(enabled);
                          break;
                      case SwitchId::ChargeQc:
                          GetHAL()->setChargeQcEnable(enabled);
                          break;
                      case SwitchId::Ext5v:
                          GetHAL()->setExt5vEnable(enabled);
                          break;
                      case SwitchId::UsbA5v:
                          GetHAL()->setUsb5vEnable(enabled);
                          break;
                      case SwitchId::ExtAntenna:
                          GetHAL()->setExtAntennaEnable(enabled);
                          break;
                  }
              },
              [] { return GetHAL()->getInterfaceData(); })
    {
        setAppInfo().name = "Switches";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Switches", [&]() { close(); });
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
        refreshInterface();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _bindings.clear();
        _shell.reset();
    }

private:
    SwitchesController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<ToggleBinding>> _bindings;
    lv_obj_t* _status      = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeSwitchesLayout();

        auto* description = app_ui::makeLabel(
            content(), "Control power rails and interface routing from one safe panel.", layout.description.x,
            layout.description.y, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);

        const auto switches = _controller.readSwitches();
        addToggle(layout, 0, "Charge Enable", "Allow battery charging", SwitchId::Charge, switches.charge);
        addToggle(layout, 1, "Charge QC", "Enable quick-charge negotiation", SwitchId::ChargeQc, switches.chargeQc);
        addToggle(layout, 2, "External 5V", "Drive the external 5V rail", SwitchId::Ext5v, switches.ext5v);
        addToggle(layout, 3, "USB-A 5V", "Power the USB-A host port", SwitchId::UsbA5v, switches.usbA5v);
        addToggle(layout, 4, "External Antenna", "Route wireless through the external antenna", SwitchId::ExtAntenna,
                  switches.extAntenna);

        auto* panel = app_ui::makePanel(content(), layout.interfaceStatus.x, layout.interfaceStatus.y,
                                        layout.interfaceStatus.w, layout.interfaceStatus.h);
        auto* title = app_ui::makeLabel(panel, "Interface Status", layout.statusTitle.x, layout.statusTitle.y,
                                        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, layout.statusTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        _status = app_ui::makeLabel(panel, "", layout.statusDetail.x, layout.statusDetail.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_status, layout.statusDetail.w);
        lv_label_set_long_mode(_status, LV_LABEL_LONG_DOT);
        refreshInterface();
    }

    void addToggle(const SwitchesLayout& layout, int index, const char* title, const char* detail, SwitchId id,
                   bool initial)
    {
        const auto card  = switchCardRect(layout, index);
        auto* panel      = app_ui::makePanel(content(), card.x, card.y, card.w, card.h);
        auto* titleLabel = app_ui::makeLabel(panel, title, layout.cardTitle.x, layout.cardTitle.y,
                                             &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(titleLabel, layout.cardTitle.w);
        lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
        auto* detailLabel = app_ui::makeLabel(panel, detail, layout.cardDetail.x, layout.cardDetail.y,
                                              &lv_font_montserrat_14, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(detailLabel, layout.cardDetail.w);
        lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_DOT);
        auto* sw = app_ui::makeSwitch(panel, layout.switchControl.x, layout.switchControl.y, layout.switchControl.w,
                                      layout.switchControl.h, initial);

        auto binding = std::make_unique<ToggleBinding>();
        binding->app = this;
        binding->id  = id;
        lv_obj_add_event_cb(sw, AppSwitches::onToggle, LV_EVENT_VALUE_CHANGED, binding.get());
        _bindings.push_back(std::move(binding));
    }

    void setSwitch(SwitchId id, bool enabled)
    {
        audio::play_next_tone_progression();
        _controller.setSwitch(id, enabled);
        refreshInterface();
    }

    void refreshInterface()
    {
        if (_status == nullptr) {
            return;
        }
        const auto text = SwitchesController::formatInterfaceStatus(_controller.readInterface());
        lv_label_set_text(_status, text.c_str());
    }

    static void onToggle(lv_event_t* event)
    {
        auto* binding = static_cast<ToggleBinding*>(lv_event_get_user_data(event));
        if (binding == nullptr || binding->app == nullptr) {
            return;
        }
        auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
        binding->app->setSwitch(binding->id, lv_obj_has_state(target, LV_STATE_CHECKED));
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_switches::makeSwitchesApp()
{
    return std::make_unique<AppSwitches>();
}

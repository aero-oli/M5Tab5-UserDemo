/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_gpio.h"
#include "gpio_controller.h"
#include "gpio_layout.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_hardware_gpio;

namespace {

struct PinBinding {
    class AppGpio* app = nullptr;
    uint8_t pin = 0;
    lv_obj_t* state = nullptr;
    lv_obj_t* toggle = nullptr;
    lv_obj_t* action = nullptr;
};

class AppGpio : public mooncake::AppAbility {
public:
    AppGpio() : _controller(GetHAL()->gpioGetPins(),
                           [](uint8_t pin) { return GetHAL()->gpioInitOutput(pin); },
                           [](uint8_t pin, bool level) { return GetHAL()->gpioSetLevel(pin, level); },
                           [](uint8_t pin) { return GetHAL()->gpioReset(pin); })
    {
        setAppInfo().name = "GPIO Test";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("GPIO Test", [&] {
            if (_controller.resetAll()) {
                close();
            } else {
                refresh();
            }
        });
        _shell->init();
        build();
    }

    void onClose() override
    {
        _controller.resetAll();
        _shell.reset();
        _bindings.clear();
    }

private:
    GpioController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<PinBinding>> _bindings;
    lv_obj_t* _status = nullptr;
    lv_obj_t* _error = nullptr;
    lv_obj_t* _releaseAll = nullptr;
    bool _syncing = false;

    static lv_obj_t* fittedText(lv_obj_t* parent, const char* text, const app_ui::Rect& rect, const lv_font_t* font)
    {
        return app_ui::makeFittedLabel(parent, text, rect.x, rect.y, rect.w, rect.h, font,
                                      app_ui::theme::color(app_ui::theme::text), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
    }

    static void setText(lv_obj_t* object, const char* text)
    {
        if (std::strcmp(lv_label_get_text(object), text) != 0) {
            lv_label_set_text(object, text);
        }
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeGpioLayout(static_cast<int>(_controller.pins().size()));
        _status = fittedText(_shell->content(), "No outputs claimed", layout.statusText, &lv_font_montserrat_20);
        _error = fittedText(_shell->content(), "", layout.errorText, &lv_font_montserrat_18);
        _releaseAll = app_ui::makeButton(_shell->content(), "Release All", layout.releaseAll.x, layout.releaseAll.y,
                                         layout.releaseAll.w, layout.releaseAll.h, app_ui::ButtonKind::Danger);
        lv_obj_add_event_cb(_releaseAll, [](lv_event_t* event) {
            auto* app = static_cast<AppGpio*>(lv_event_get_user_data(event));
            app->_controller.resetAll();
            app->refresh();
        }, LV_EVENT_CLICKED, this);
        auto* grid = app_ui::makeScrollableContent(_shell->content(), layout.grid.x, layout.grid.y, layout.grid.w, layout.grid.h);
        if (_controller.pins().empty()) {
            fittedText(grid, "GPIO unavailable", {0, 0, layout.grid.w, 28}, &lv_font_montserrat_20);
        }
        for (size_t index = 0; index < _controller.pins().size(); ++index) {
            const auto& pin = _controller.pins()[index].info;
            const auto rect = gpioCardRect(layout, static_cast<int>(index));
            auto* panel = app_ui::makePanel(grid, rect.x, rect.y, rect.w, rect.h);
            char title[24];
            std::snprintf(title, sizeof(title), "GPIO %u", static_cast<unsigned>(pin.pin));
            fittedText(panel, title, layout.title, &lv_font_montserrat_18);
            fittedText(panel, pin.outputAllowed ? pin.role.c_str() : pin.reservation.c_str(), layout.role, &lv_font_montserrat_14);
            auto binding = std::make_unique<PinBinding>();
            binding->app = this;
            binding->pin = pin.pin;
            binding->state = fittedText(panel, "Unclaimed", layout.state, &lv_font_montserrat_16);
            binding->toggle = app_ui::makeSwitch(panel, layout.toggle.x, layout.toggle.y, layout.toggle.w, layout.toggle.h);
            binding->action = app_ui::makeButton(panel, "Drive Low", layout.action.x, layout.action.y, layout.action.w,
                                                 layout.action.h, app_ui::ButtonKind::Secondary);
            lv_obj_add_event_cb(binding->action, onAction, LV_EVENT_CLICKED, binding.get());
            lv_obj_add_event_cb(binding->toggle, onToggle, LV_EVENT_VALUE_CHANGED, binding.get());
            _bindings.push_back(std::move(binding));
        }
        refresh();
    }

    void refresh()
    {
        LvglLockGuard lock;
        _syncing = true;
        for (const auto& binding : _bindings) {
            const auto* state = _controller.state(binding->pin);
            const auto* text = !state->info.outputAllowed ? "Protected" : !state->claimed ? "Unclaimed" :
                               !state->levelKnown ? "Unknown" : state->high ? "High" : "Low";
            setText(binding->state, text);
            setText(lv_obj_get_child(binding->action, 0), !state->info.outputAllowed ? "Reserved" :
                                                        state->claimed ? "Release" : "Drive Low");
            if (!state->info.outputAllowed) {
                lv_obj_add_state(binding->action, LV_STATE_DISABLED);
                lv_obj_add_flag(binding->toggle, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(binding->toggle, LV_OBJ_FLAG_HIDDEN);
                if (state->claimed && state->levelKnown) {
                    lv_obj_remove_state(binding->toggle, LV_STATE_DISABLED);
                } else {
                    lv_obj_add_state(binding->toggle, LV_STATE_DISABLED);
                }
            }
            if (state->claimed && state->high) {
                lv_obj_add_state(binding->toggle, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(binding->toggle, LV_STATE_CHECKED);
            }
        }
        const auto count = _controller.claimedCount();
        char status[80];
        if (count == 0) {
            std::snprintf(status, sizeof(status), "No outputs claimed");
            lv_obj_add_state(_releaseAll, LV_STATE_DISABLED);
        } else {
            std::snprintf(status, sizeof(status), "%u output%s claimed", static_cast<unsigned>(count), count == 1 ? "" : "s");
            lv_obj_remove_state(_releaseAll, LV_STATE_DISABLED);
        }
        setText(_status, status);
        setText(_error, _controller.lastError().empty() ? "Outputs may energize connected hardware" : _controller.lastError().c_str());
        lv_obj_set_style_text_color(_error, app_ui::theme::color(_controller.lastError().empty() ? app_ui::theme::amber : app_ui::theme::red), 0);
        _syncing = false;
    }

    static void onAction(lv_event_t* event)
    {
        auto* binding = static_cast<PinBinding*>(lv_event_get_user_data(event));
        auto& controller = binding->app->_controller;
        if (controller.state(binding->pin)->claimed) {
            controller.release(binding->pin);
        } else {
            controller.claim(binding->pin);
        }
        binding->app->refresh();
    }

    static void onToggle(lv_event_t* event)
    {
        auto* binding = static_cast<PinBinding*>(lv_event_get_user_data(event));
        if (!binding->app->_syncing) {
            binding->app->_controller.setLevel(binding->pin, lv_obj_has_state(binding->toggle, LV_STATE_CHECKED));
            binding->app->refresh();
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_gpio::makeGpioApp()
{
    return std::make_unique<AppGpio>();
}

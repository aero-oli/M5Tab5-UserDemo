/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings.h"
#include "about_layout.h"

#include <memory>
#include <string>
#include <utility>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/app_settings_display_sound/app_display_sound.h>
#include <apps/app_settings_power/app_power_settings.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_settings;

namespace {

class AppAbout : public mooncake::AppAbility {
public:
    AppAbout()
    {
        setAppInfo().name = "About";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("About", [&]() { close(); });
        _shell->init();
        build();
    }

    void onClose() override
    {
        _shell.reset();
    }

private:
    std::unique_ptr<app_ui::AppShell> _shell;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeAboutLayout();
        app_ui::makeMetricCard(content(), "Device", GetHAL()->type().c_str(), layout.deviceCard.x, layout.deviceCard.y,
                               layout.deviceCard.w, layout.deviceCard.h);
        app_ui::makeMetricCard(content(), "Display", "1280 x 720", layout.displayCard.x, layout.displayCard.y,
                               layout.displayCard.w, layout.displayCard.h);
        auto* panel = app_ui::makePanel(content(), layout.infoPanel.x, layout.infoPanel.y, layout.infoPanel.w,
                                        layout.infoPanel.h);
        auto* registry = app_ui::makeWrappedLabel(
            panel,
            "This build uses the modular app registry. Add future tools as app-owned views with controllers, then register metadata once.",
            layout.registryText.x,
            layout.registryText.y,
            &lv_font_montserrat_18,
            app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(registry, layout.registryText.w);
        auto* categories = app_ui::makeWrappedLabel(
            panel,
            "Settings contains device configuration. Hardware contains diagnostics and physical I/O. Modules contains add-on modules like PPS.",
            layout.categoryText.x,
            layout.categoryText.y,
            &lv_font_montserrat_18,
            app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(categories, layout.categoryText.w);
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_settings::makeDisplaySoundApp()
{
    return app_settings_display_sound::makeDisplaySoundApp();
}

std::unique_ptr<mooncake::AppAbility> app_settings::makePowerSettingsApp()
{
    return app_settings_power::makePowerSettingsApp();
}

std::unique_ptr<mooncake::AppAbility> app_settings::makeAboutApp()
{
    return std::make_unique<AppAbout>();
}

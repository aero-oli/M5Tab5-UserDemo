/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_template.h"

#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake_log.h>
#include <smooth_lvgl.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

AppTemplate::AppTemplate()
{
    setAppInfo().name = "AppTemplate";
}

void AppTemplate::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _shell = std::make_unique<app_ui::AppShell>("Template", [&]() { close(); });
    _shell->init();
    LvglLockGuard lock;
    auto* title = app_ui::makeLabel(_shell->content(), "Template App", 0, 0, &lv_font_montserrat_24,
                                    app_ui::theme::color(app_ui::theme::text));
    lv_obj_set_width(title, 640);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    auto* detail = app_ui::makeLabel(_shell->content(), "Use this as a minimal AppShell-based app skeleton.", 0, 56,
                                     &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted));
    lv_obj_set_width(detail, 900);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
}

void AppTemplate::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _shell.reset();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include "launcher_layout.h"
#include <utility>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <smooth_lvgl.h>
#include <apps/utils/audio/audio.h>
#include <apps/app_pps/pps_controller.h>
#include <apps/ui/ui_layout.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace launcher_view;
using namespace app_registry;

static const std::string _tag = "launcher-view";

LauncherView::~LauncherView()
{
    LvglLockGuard lock;
    app_ui::destroyObject(_root);
}

void LauncherView::init()
{
    mclog::tagInfo(_tag, "init");

    LvglLockGuard lock;
    lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);
    createLayout();
    renderCategories();
    renderTiles();
    requestHardwareStatus(GetHAL()->millis());
    refreshStatus();
}

void LauncherView::update()
{
    const uint32_t now = GetHAL()->millis();
    requestHardwareStatus(now);
    if (now - _lastStatusRefreshMs < 250) {
        return;
    }

    LvglLockGuard lock;
    refreshStatus();
    _lastStatusRefreshMs = now;
}

void LauncherView::requestHardwareStatus(uint32_t nowMs)
{
    if (_hardwarePolled && nowMs - _lastHardwarePollMs < 500) return;
    auto* hardware = GetHAL();
    if (_statusSampler.start("Home status", [hardware] {
        hardware->updatePowerMonitorData();
        hardware->updateRtcData();
        hardware->updateInterfaceData();
    })) {
        _hardwarePolled = true;
        _lastHardwarePollMs = nowMs;
    }
}

void LauncherView::createLayout()
{
    auto layout = makeLauncherLayout();
    _root = app_ui::makeScreenRoot(GetHAL()->getDisplayWidth(), GetHAL()->getDisplayHeight());

    _topBar = app_ui::makePanel(_root, layout.topBar.x, layout.topBar.y, layout.topBar.w, layout.topBar.h);

    lv_obj_set_style_pad_all(_topBar, 0, 0);
    const auto& titleRect = layout.headerTitle;
    app_ui::makeFittedLabel(_topBar, "M5Tab5 Home", titleRect.x, titleRect.y, titleRect.w, titleRect.h,
        &lv_font_montserrat_24, app_ui::theme::color(app_ui::theme::text), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
    const auto& statusRect = layout.headerStatus;
    _statusLabel = app_ui::makeFittedLabel(_topBar, "Network: --", statusRect.x, statusRect.y, statusRect.w, statusRect.h,
        &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_CENTER);
    const auto& timeRect = layout.headerTime;
    _timeLabel = app_ui::makeFittedLabel(_topBar, "--:--", timeRect.x, timeRect.y, timeRect.w, timeRect.h,
        &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_RIGHT);

    _categoryRail =
        app_ui::makePanel(_root, layout.categoryRail.x, layout.categoryRail.y, layout.categoryRail.w, layout.categoryRail.h);
    lv_obj_set_style_pad_all(_categoryRail, 0, 0);

    _tileArea = app_ui::makePanel(_root, layout.tileArea.x, layout.tileArea.y, layout.tileArea.w, layout.tileArea.h);
    lv_obj_set_style_pad_all(_tileArea, 0, 0);
    lv_obj_set_style_bg_color(_tileArea, app_ui::theme::color(0x10171D), 0);
    lv_obj_add_flag(_tileArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_tileArea, LV_SCROLLBAR_MODE_AUTO);
}

void LauncherView::renderCategories()
{
    lv_obj_clean(_categoryRail);
    _categoryActions.clear();
    auto layout = makeLauncherLayout();

    auto* heading = app_ui::makeLabel(_categoryRail, "Areas", 0, 0, &lv_font_montserrat_22, app_ui::theme::color(app_ui::theme::text));
    lv_obj_set_pos(heading, layout.categoryHeadingX, layout.categoryHeadingY);

    int index = 0;
    for (auto category : categories()) {
        auto action = std::make_unique<CategoryAction>();
        action->view = this;
        action->category = category;

        auto rect = launcherCategoryButtonRect(layout, index);
        auto* button = app_ui::makeButtonSurface(_categoryRail,
                                                 rect.x,
                                                 rect.y,
                                                 rect.w,
                                                 rect.h,
                                                 app_ui::theme::color(app_ui::theme::panelAlt),
                                                 app_ui::theme::color(app_ui::theme::border),
                                                 1,
                                                 7);
        lv_obj_add_event_cb(button, LauncherView::onCategoryClicked, LV_EVENT_CLICKED, action.get());

        app_ui::makeFittedLabel(button,
                                categoryName(category),
                                12,
                                (rect.h - 22) / 2,
                                rect.w - 24,
                                22,
                                &lv_font_montserrat_16,
                                app_ui::theme::color(app_ui::theme::text),
                                LV_LABEL_LONG_DOT,
                                LV_TEXT_ALIGN_CENTER);

        action->button = button;
        _categoryActions.push_back(std::move(action));
        ++index;
    }
    refreshCategoryStyles();
}

void LauncherView::refreshCategoryStyles()
{
    for (const auto& action : _categoryActions) {
        if (action == nullptr || action->button == nullptr) {
            continue;
        }

        const bool selected = action->category == _selectedCategory;
        lv_obj_set_style_bg_color(action->button,
                                  selected ? app_ui::theme::color(app_ui::theme::blue)
                                           : app_ui::theme::color(app_ui::theme::panelAlt),
                                  0);
        lv_obj_set_style_border_color(action->button,
                                      selected ? app_ui::theme::color(0x74B8FF)
                                               : app_ui::theme::color(app_ui::theme::border),
                                      0);
        lv_obj_set_style_shadow_width(action->button, selected ? 10 : 0, 0);
        lv_obj_set_style_shadow_color(action->button, app_ui::theme::color(0x0B5CAD), 0);
        lv_obj_set_style_shadow_opa(action->button, selected ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

void LauncherView::renderTiles()
{
    lv_obj_clean(_tileArea);
    _tileActions.clear();

    auto apps = _selectedCategory == AppCategory::Home ? registry().dashboardApps() : registry().byCategory(_selectedCategory);
    if (apps.empty()) {
        _emptyLabel =
            app_ui::makeLabel(_tileArea,
                              _selectedCategory == AppCategory::Home ? "No dashboard apps available"
                                                                     : "No apps in this category",
                              0,
                              0,
                              &lv_font_montserrat_20,
                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_center(_emptyLabel);
        return;
    }

    auto* heading = app_ui::makeLabel(_tileArea,
                                      _selectedCategory == AppCategory::Home ? "Dashboard" : categoryName(_selectedCategory),
                                      0,
                                      0,
                                      &lv_font_montserrat_24,
                                      app_ui::theme::color(app_ui::theme::text));
    auto layout = makeLauncherLayout();
    lv_obj_set_pos(heading, layout.tileHeadingX, layout.tileHeadingY);

    int index = 0;
    for (const auto* app : apps) {
        if (_selectedCategory == AppCategory::Home && index >= 6) {
            break;
        }
        auto rect = launcherTileRect(layout, index);

        auto action = std::make_unique<TileAction>();
        action->view = this;
        action->key = app->key;

        auto* tile = app_ui::makeButtonSurface(_tileArea,
                                               rect.x,
                                               rect.y,
                                               rect.w,
                                               rect.h,
                                               app_ui::theme::color(app_ui::theme::panelAlt),
                                               app->dangerous ? app_ui::theme::color(app_ui::theme::red)
                                                              : app_ui::theme::color(app_ui::theme::border),
                                               1,
                                               8);
        lv_obj_add_event_cb(tile, LauncherView::onTileClicked, LV_EVENT_CLICKED, action.get());

        const char* initials = app->icon.empty() ? "?" : app->icon.c_str();
        app_ui::makeIconBadge(tile,
                              initials,
                              layout.tileIcon.x,
                              layout.tileIcon.y,
                              layout.tileIcon.w,
                              layout.tileIcon.h,
                              app->dangerous ? app_ui::theme::color(app_ui::theme::red)
                                             : app_ui::theme::color(app_ui::theme::blue));

        app_ui::makeFittedLabel(tile, app->label.c_str(), layout.tileTitle.x, layout.tileTitle.y,
            layout.tileTitle.w, layout.tileTitle.h, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text),
            LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        app_ui::makeFittedLabel(tile, app->description.c_str(), layout.tileDescription.x, layout.tileDescription.y,
            layout.tileDescription.w, layout.tileDescription.h, &lv_font_montserrat_16,
            app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);

        const auto status = registry().status(app->key);
        const auto text = status.text.empty() ? app->defaultStatus : status.text;
        action->status = app_ui::makeFittedLabel(tile, text.c_str(), layout.tileStatus.x, layout.tileStatus.y,
            layout.tileStatus.w, layout.tileStatus.h, &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::muted),
            LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        action->lastStatus = text;

        _tileActions.push_back(std::move(action));
        ++index;
    }
}

void LauncherView::refreshStatus()
{
    const auto clockStatus = registry().status(AppKey::Rtc);
    const auto& timeText = clockStatus.text;
    lv_obj_set_style_text_color(_timeLabel, app_ui::theme::color(clockStatus.level == StatusLevel::Error ? app_ui::theme::red :
        clockStatus.level == StatusLevel::Warning ? app_ui::theme::amber : app_ui::theme::text), 0);
    if (_lastTimeText != timeText) {
        lv_label_set_text(_timeLabel, timeText.c_str());
        _lastTimeText = timeText;
    }

    for (auto& tile : _tileActions) {
        if (!tile->status) continue;
        const auto status = registry().status(tile->key);
        const auto* app = registry().find(tile->key);
        const auto text = status.text.empty() && app ? app->defaultStatus : status.text;
        if (tile->lastStatus != text) {
            lv_label_set_text(tile->status, text.c_str());
            tile->lastStatus = text;
        }
        const auto color = status.level == StatusLevel::Error ? app_ui::theme::red :
            status.level == StatusLevel::Warning ? app_ui::theme::amber :
            status.level == StatusLevel::Ok ? app_ui::theme::green : app_ui::theme::muted;
        lv_obj_set_style_text_color(tile->status, app_ui::theme::color(color), 0);
    }

    const auto network = registry().status(AppKey::Network);
    std::string statusText = "Network: " + network.text;
    const auto pps = app_pps::PpsController::backgroundState();
    const bool ppsOffAlert = pps && (pps->off == app_pps::OffState::Pending || pps->off == app_pps::OffState::Unconfirmed);
    if (ppsOffAlert) {
        statusText = std::string(pps->off == app_pps::OffState::Pending ? "PPS off pending" : "PPS OFF UNCONFIRMED") + " - open PPS";
    }
    const auto statusColor = ppsOffAlert || network.level == StatusLevel::Error ? app_ui::theme::red :
        network.level == StatusLevel::Warning ? app_ui::theme::amber :
        network.level == StatusLevel::Ok ? app_ui::theme::green : app_ui::theme::muted;
    lv_obj_set_style_text_color(_statusLabel, app_ui::theme::color(statusColor), 0);
    if (_lastStatusText != statusText) {
        lv_label_set_text(_statusLabel, statusText.c_str());
        _lastStatusText = statusText;
    }
}

void LauncherView::selectCategory(AppCategory category)
{
    if (_selectedCategory == category) {
        return;
    }
    _selectedCategory = category;
    refreshCategoryStyles();
    renderTiles();
}

void LauncherView::openApp(AppKey key)
{
    audio::play_next_tone_progression();
    if (!registry().open(key)) {
        mclog::tagWarn(_tag, "app not available");
    }
}

void LauncherView::onCategoryClicked(lv_event_t* event)
{
    auto* action = static_cast<CategoryAction*>(lv_event_get_user_data(event));
    action->view->selectCategory(action->category);
}

void LauncherView::onTileClicked(lv_event_t* event)
{
    auto* action = static_cast<TileAction*>(lv_event_get_user_data(event));
    action->view->openApp(action->key);
}

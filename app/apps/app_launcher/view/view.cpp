/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include "launcher_layout.h"
#include <cstdio>
#include <utility>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <smooth_lvgl.h>
#include <apps/utils/audio/audio.h>
#include <apps/runtime/performance_telemetry.h>
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
    buildSummary();
    renderSummary();
}

void LauncherView::update()
{
    const uint32_t now = GetHAL()->millis();
    if (now - _lastSummaryRefreshMs < 250) {
        return;
    }

    LvglLockGuard lock;
    renderSummary();
    _lastSummaryRefreshMs = now;
}

void LauncherView::createLayout()
{
    auto layout = makeLauncherLayout();
    _root = app_ui::makeScreenRoot(GetHAL()->getDisplayWidth(), GetHAL()->getDisplayHeight());

    _topBar = app_ui::makePanel(_root, layout.topBar.x, layout.topBar.y, layout.topBar.w, layout.topBar.h);

    auto* title = app_ui::makeLabel(_topBar, "M5Tab5 Home", 0, 0, &lv_font_montserrat_24, lv_color_hex(0xF2F6FA));
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 24, 0);

    _statusLabel = app_ui::makeLabel(_topBar, "Ready", 0, 0, &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted));
    lv_obj_align(_statusLabel, LV_ALIGN_CENTER, 40, 0);

    _timeLabel = app_ui::makeLabel(_topBar, "--:--", 0, 0, &lv_font_montserrat_20, app_ui::theme::color(app_ui::theme::text));
    lv_obj_align(_timeLabel, LV_ALIGN_RIGHT_MID, -24, 0);

    _categoryRail =
        app_ui::makePanel(_root, layout.categoryRail.x, layout.categoryRail.y, layout.categoryRail.w, layout.categoryRail.h);
    lv_obj_set_style_pad_all(_categoryRail, 0, 0);

    _tileArea = app_ui::makePanel(_root, layout.tileArea.x, layout.tileArea.y, layout.tileArea.w, layout.tileArea.h);
    lv_obj_set_style_pad_all(_tileArea, 0, 0);
    lv_obj_set_style_bg_color(_tileArea, app_ui::theme::color(0x10171D), 0);
    lv_obj_add_flag(_tileArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_tileArea, LV_SCROLLBAR_MODE_AUTO);

    _summaryArea =
        app_ui::makePanel(_root, layout.summaryArea.x, layout.summaryArea.y, layout.summaryArea.w, layout.summaryArea.h);
    lv_obj_set_style_pad_all(_summaryArea, 0, 0);
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

        auto* title = app_ui::makeLabel(tile, app->label.c_str(), 0, 0, &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_pos(title, layout.tileTitle.x, layout.tileTitle.y);
        lv_obj_set_width(title, layout.tileTitle.w);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

        auto* desc =
            app_ui::makeLabel(tile, app->description.c_str(), 0, 0, &lv_font_montserrat_14, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_pos(desc, layout.tileDescription.x, layout.tileDescription.y);
        lv_obj_set_width(desc, layout.tileDescription.w);
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);

        auto status = registry().status(app->key);
        auto* statusLabel = app_ui::makeLabel(tile,
                                              status.text.empty() ? app->defaultStatus.c_str() : status.text.c_str(),
                                              0,
                                              0,
                                              &lv_font_montserrat_14,
                                              app_ui::theme::color(0x8FBDE6));
        lv_obj_set_pos(statusLabel, layout.tileStatus.x, layout.tileStatus.y);
        lv_obj_set_width(statusLabel, layout.tileStatus.w);
        lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);

        _tileActions.push_back(std::move(action));
        ++index;
    }
}

void LauncherView::buildSummary()
{
    if (_summaryArea == nullptr) {
        return;
    }
    lv_obj_clean(_summaryArea);
    _summaryRows.clear();
    auto layout = makeLauncherLayout();

    auto* heading = app_ui::makeLabel(_summaryArea, "Live Summary", 0, 0, &lv_font_montserrat_22, app_ui::theme::color(app_ui::theme::text));
    lv_obj_set_pos(heading, layout.summaryHeadingX, layout.summaryHeadingY);

    int shown = 0;
    for (const auto* app : registry().dashboardApps()) {
        if (shown >= 10) {
            break;
        }

        auto* label =
            app_ui::makeLabel(_summaryArea, app->label.c_str(), 0, 0, &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::text));
        auto labelRect = launcherSummaryLabelRect(layout, shown);
        lv_obj_set_pos(label, labelRect.x, labelRect.y);
        lv_obj_set_width(label, labelRect.w);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

        auto* value = app_ui::makeLabel(_summaryArea, "--", 0, 0, &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::muted));
        auto valueRect = launcherSummaryValueRect(layout, shown);
        lv_obj_set_pos(value, valueRect.x, valueRect.y);
        lv_obj_set_width(value, valueRect.w);
        lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);

        SummaryRow row;
        row.key = app->key;
        row.label = label;
        row.value = value;
        _summaryRows.push_back(std::move(row));
        ++shown;
    }
}

void LauncherView::renderSummary()
{
    if (_summaryArea == nullptr) {
        return;
    }
    if (_summaryRows.empty()) {
        buildSummary();
    }

    tm now = {};
    GetHAL()->getRtcTime(&now);
    char timeText[16];
    std::snprintf(timeText, sizeof(timeText), "%02d:%02d", now.tm_hour, now.tm_min);
    if (_lastTimeText != timeText) {
        lv_label_set_text(_timeLabel, timeText);
        _lastTimeText = timeText;
    }

    for (auto& row : _summaryRows) {
        auto* app = registry().find(row.key);
        if (app == nullptr || row.value == nullptr) {
            continue;
        }
        auto status = registry().status(app->key);
        std::string text = status.text.empty() ? app->defaultStatus : status.text;
        if (text.empty()) {
            text = "--";
        }

        lv_color_t color = app_ui::theme::color(app_ui::theme::muted);
        if (status.level == StatusLevel::Ok) {
            color = app_ui::theme::color(app_ui::theme::green);
        } else if (status.level == StatusLevel::Warning) {
            color = app_ui::theme::color(app_ui::theme::amber);
        } else if (status.level == StatusLevel::Error) {
            color = app_ui::theme::color(app_ui::theme::red);
        }

        if (row.lastValue != text) {
            lv_label_set_text(row.value, text.c_str());
            row.lastValue = text;
        }
        if (row.lastLevel != status.level) {
            lv_obj_set_style_text_color(row.value, color, 0);
            row.lastLevel = status.level;
        }
    }

    auto perf = app_runtime::globalPerformanceTelemetry().snapshot();
    char statusText[160];
    if (perf.backgroundBusy && !perf.backgroundJob.empty()) {
        std::snprintf(statusText, sizeof(statusText), "Task: %s  UI %ums avg %ums", perf.backgroundJob.c_str(),
                      static_cast<unsigned>(perf.lastAppUpdateMs), static_cast<unsigned>(perf.avgAppUpdateMs));
    } else {
        std::snprintf(statusText, sizeof(statusText), "%d apps  UI %ums avg %ums slow %u",
                      static_cast<int>(registry().all().size()), static_cast<unsigned>(perf.lastAppUpdateMs),
                      static_cast<unsigned>(perf.avgAppUpdateMs), static_cast<unsigned>(perf.slowAppUpdates));
    }
    const auto pps = app_pps::PpsController::backgroundState();
    const bool ppsOffAlert = pps && (pps->off == app_pps::OffState::Pending || pps->off == app_pps::OffState::Unconfirmed);
    if (ppsOffAlert) {
        std::snprintf(statusText, sizeof(statusText), "%s - open PPS",
                      pps->off == app_pps::OffState::Pending ? "PPS off pending" : "PPS OFF UNCONFIRMED");
    }
    lv_obj_set_style_text_color(_statusLabel,
        app_ui::theme::color(ppsOffAlert ? app_ui::theme::red : app_ui::theme::muted), 0);
    if (_lastStatusText != statusText) {
        lv_label_set_text(_statusLabel, statusText);
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

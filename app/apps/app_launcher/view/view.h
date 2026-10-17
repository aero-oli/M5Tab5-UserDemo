/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <lvgl.h>
#include <apps/app_registry.h>
#include <apps/runtime/async_job_runner.h>

namespace launcher_view {

class LauncherView {
public:
    ~LauncherView();

    void init();
    void update();

private:
    struct CategoryAction {
        LauncherView* view = nullptr;
        app_registry::AppCategory category = app_registry::AppCategory::Home;
        lv_obj_t* button = nullptr;
    };

    struct TileAction {
        LauncherView* view = nullptr;
        app_registry::AppKey key = app_registry::AppKey::Rtc;
        lv_obj_t* status = nullptr;
        std::string lastStatus;
    };

    lv_obj_t* _root = nullptr;
    lv_obj_t* _topBar = nullptr;
    lv_obj_t* _categoryRail = nullptr;
    lv_obj_t* _tileArea = nullptr;
    lv_obj_t* _timeLabel = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    lv_obj_t* _emptyLabel = nullptr;
    app_registry::AppCategory _selectedCategory = app_registry::AppCategory::Home;
    std::vector<std::unique_ptr<CategoryAction>> _categoryActions;
    std::vector<std::unique_ptr<TileAction>> _tileActions;
    std::string _lastStatusText;
    std::string _lastTimeText;
    uint32_t _lastStatusRefreshMs = 0;
    uint32_t _lastHardwarePollMs = 0;
    bool _hardwarePolled = false;
    app_runtime::AsyncJobRunner _statusSampler;

    void createLayout();
    void renderCategories();
    void refreshCategoryStyles();
    void renderTiles();
    void refreshStatus();
    void requestHardwareStatus(uint32_t nowMs);
    void selectCategory(app_registry::AppCategory category);
    void openApp(app_registry::AppKey key);

    static void onCategoryClicked(lv_event_t* event);
    static void onTileClicked(lv_event_t* event);
};

}  // namespace launcher_view

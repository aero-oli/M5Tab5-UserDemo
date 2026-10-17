/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool hasSuffix(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string normalisedRelativePath(const fs::path& root, const fs::path& path)
{
    auto relative = fs::relative(path, root).generic_string();
    return relative;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

std::string functionBodyFrom(const std::string& source, const std::string& signature)
{
    const auto start = source.find(signature);
    if (start == std::string::npos) {
        return {};
    }

    const auto bodyStart = source.find('{', start);
    if (bodyStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (auto i = bodyStart; i < source.size(); ++i) {
        if (source[i] == '{') {
            ++depth;
        } else if (source[i] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(bodyStart, i - bodyStart + 1);
            }
        }
    }
    return {};
}

}  // namespace

int main()
{
    const fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path();
    const fs::path appsRoot = repoRoot / "app" / "apps";
    const std::set<std::string> allowedScreenRoots = {
        "app/apps/ui/ui_components.cpp",
        "app/apps/ui/app_shell.cpp",
        "app/apps/app_launcher/view/view.cpp",
        "app/apps/utils/ui/toast.cpp",
    };
    const std::set<std::string> allowedRawButtons = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawLabels = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawTextAreas = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawSwitches = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawSliders = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawBars = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedRawDeletes = {
        "app/apps/ui/ui_components.cpp",
    };
    const std::set<std::string> allowedScreenRootCreation = {
        "app/apps/ui/ui_components.cpp",
    };

    std::vector<std::string> failures;
    for (const auto& entry : fs::recursive_directory_iterator(appsRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        const auto pathText = path.generic_string();
        if (!hasSuffix(pathText, ".cpp") && !hasSuffix(pathText, ".h")) {
            continue;
        }

        const auto relative = normalisedRelativePath(repoRoot, path);
        const auto source = readFile(path);

        if (contains(source, "lv_screen_active(") && allowedScreenRoots.count(relative) == 0) {
            failures.push_back(relative + " creates UI directly on lv_screen_active()");
        }
        if (contains(source, "lv_obj_create(lv_screen_active())") && allowedScreenRootCreation.count(relative) == 0) {
            failures.push_back(relative + " creates app roots directly instead of using shared UI screen roots");
        }
        if (contains(source, "lv_button_create(") && allowedRawButtons.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL buttons instead of shared UI primitives");
        }
        if (contains(source, "lv_label_create(") && allowedRawLabels.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL labels instead of shared UI primitives");
        }
        if (contains(source, "lv_textarea_create(") && allowedRawTextAreas.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL textareas instead of shared UI primitives");
        }
        if (contains(source, "lv_switch_create(") && allowedRawSwitches.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL switches instead of shared UI primitives");
        }
        if (contains(source, "lv_slider_create(") && allowedRawSliders.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL sliders instead of shared UI primitives");
        }
        if (contains(source, "lv_bar_create(") && allowedRawBars.count(relative) == 0) {
            failures.push_back(relative + " creates raw LVGL bars instead of shared UI primitives");
        }
        if (contains(source, "lv_obj_delete(") && allowedRawDeletes.count(relative) == 0) {
            failures.push_back(relative + " deletes raw LVGL objects instead of using shared UI cleanup");
        }
        if (contains(source, "static lv_obj_t* make_label") || contains(source, "static lv_obj_t* label(")) {
            failures.push_back(relative + " defines local label helpers instead of shared UI primitives");
        }
        if (contains(source, "static lv_obj_t* make_panel") || contains(source, "static void style_panel")) {
            failures.push_back(relative + " defines local panel styling instead of shared UI primitives");
        }
        if (contains(source, "PanelHostApp") || contains(source, "legacy_panels")) {
            failures.push_back(relative + " references legacy panel hosting");
        }
        if (path.filename().generic_string().rfind("panel_", 0) == 0) {
            failures.push_back(relative + " is a legacy launcher panel file");
        }
    }

    const auto launcherSource = readFile(appsRoot / "app_launcher" / "view" / "view.cpp");
    const auto renderTiles = functionBodyFrom(launcherSource, "void LauncherView::renderTiles()");
    if (renderTiles.empty()) {
        failures.push_back("LauncherView::renderTiles() was not found");
    } else if (contains(renderTiles, "lv_obj_create(")) {
        failures.push_back("LauncherView::renderTiles() must use shared UI primitives for tile internals");
    }

    const auto renderSummary = functionBodyFrom(launcherSource, "void LauncherView::refreshStatus()");
    if (renderSummary.empty()) {
        failures.push_back("LauncherView::refreshStatus() was not found");
    } else if (contains(renderSummary, "lv_obj_clean(")) {
        failures.push_back("LauncherView::refreshStatus() must update labels in place, not rebuild objects");
    }

    const auto selectCategory = functionBodyFrom(launcherSource, "void LauncherView::selectCategory(AppCategory category)");
    if (selectCategory.empty()) {
        failures.push_back("LauncherView::selectCategory() was not found");
    } else if (contains(selectCategory, "renderCategories(")) {
        failures.push_back("LauncherView::selectCategory() must update category button state in place, not rebuild the rail");
    }

    const auto installerSource = readFile(appsRoot / "app_installer.h");
    if (contains(installerSource, "on_startup_anim") || contains(installerSource, "AppStartupAnim")) {
        failures.push_back("app_installer.h must not block boot on the old startup animation");
    }

    const auto assetHeader = readFile(repoRoot / "app" / "assets" / "assets.h");
    const std::vector<std::string> retiredAssets = {
        "launcher_bg",
        "logo_tab",
        "logo_5",
        "sw_chg_",
        "sw_qc_",
        "sw_rf_",
        "internal_i2c_dev_chart",
        "porta_i2c_dev_chart",
        "porta_i2c_ext5v_on",
        "chg_arrow_",
        "arrow_state_on",
    };
    for (const auto& asset : retiredAssets) {
        if (contains(assetHeader, asset)) {
            failures.push_back("assets.h exposes retired legacy bitmap asset: " + asset);
        }
    }

    const auto appRuntimeSource = readFile(repoRoot / "platforms" / "desktop" / "test_support" / "ui_smoke.cpp");
    if (!contains(appRuntimeSource, "desktopValidateNonScrollableBounds(")) {
        failures.push_back("desktop runtime smoke must validate non-scrollable LVGL object bounds");
    }
    const auto allAppsSmoke = functionBodyFrom(appRuntimeSource, "static void desktopAssertAllRegisteredAppsUi()");
    if (allAppsSmoke.empty()) {
        failures.push_back("desktopAssertAllRegisteredAppsUi() was not found");
    } else if (!contains(allAppsSmoke, "desktopValidateNonScrollableBounds(")) {
        failures.push_back("desktop all-app UI smoke must reject off-bounds controls");
    }

    if (!failures.empty()) {
        for (const auto& failure : failures) {
            std::cerr << failure << '\n';
        }
        return 1;
    }

    return 0;
}

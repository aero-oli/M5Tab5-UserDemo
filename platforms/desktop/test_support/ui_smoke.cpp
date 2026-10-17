/* SPDX-License-Identifier: MIT */
#include "ui_smoke.h"
#include <apps/app_registry.h>
#include <apps/runtime/async_job_runner.h>
#include <apps/ui/ui_layout.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace mooncake;
namespace {
const std::string _tag = "desktop-smoke";
void (*_pumpLifecycle)() = nullptr;

// Smoke journeys drive the same timer as normal runtime; they never bypass its owner.
static void desktopPumpLifecycle()
{
    _pumpLifecycle();
}

static uint32_t _desktopUpdateCount = 0;
static uint32_t _desktopMaxUpdates = 0;
static bool _desktopAssertLauncherUi = false;
static bool _desktopLauncherUiAsserted = false;
static bool _desktopAssertAllAppsUi = false;
static bool _desktopAllAppsUiAsserted = false;

static uint32_t desktopMaxUpdatesFromEnv()
{
    const char* raw = std::getenv("M5TAB5_DESKTOP_MAX_FRAMES");
    if (raw == nullptr || raw[0] == '\0') {
        return 0;
    }

    char* end = nullptr;
    const auto parsed = std::strtoul(raw, &end, 10);
    if (end == raw || parsed == 0 || parsed > UINT32_MAX) {
        return 0;
    }
    return static_cast<uint32_t>(parsed);
}

static bool desktopEnvEnabled(const char* name)
{
    const char* raw = std::getenv(name);
    return raw != nullptr && raw[0] != '\0' && raw[0] != '0';
}

struct DesktopUiSnapshot {
    uint32_t objectCount = 0;
    uint32_t fullScreenChildCount = 0;
};

static lv_obj_tree_walk_res_t desktopCountUiObjects(lv_obj_t* obj, void* userData)
{
    auto* snapshot = static_cast<DesktopUiSnapshot*>(userData);
    ++snapshot->objectCount;

    const auto displayWidth = GetHAL()->getDisplayWidth();
    const auto displayHeight = GetHAL()->getDisplayHeight();
    if (lv_obj_get_parent(obj) == lv_screen_active() && lv_obj_get_width(obj) >= displayWidth &&
        lv_obj_get_height(obj) >= displayHeight) {
        ++snapshot->fullScreenChildCount;
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

static bool desktopLauncherUiLooksReady()
{
    LvglLockGuard lock;
    auto* screen = lv_screen_active();
    if (screen == nullptr || lv_obj_get_child_count(screen) == 0) {
        return false;
    }
    lv_obj_update_layout(screen);

    lv_obj_t* topRoot = nullptr;
    const auto displayWidth = GetHAL()->getDisplayWidth();
    const auto displayHeight = GetHAL()->getDisplayHeight();
    for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(screen)) - 1; i >= 0; --i) {
        auto* child = lv_obj_get_child(screen, static_cast<uint32_t>(i));
        if (child != nullptr && lv_obj_get_width(child) >= displayWidth && lv_obj_get_height(child) >= displayHeight) {
            topRoot = child;
            break;
        }
    }
    if (topRoot == nullptr || lv_obj_get_child_count(topRoot) < 3) {
        return false;
    }

    DesktopUiSnapshot snapshot;
    lv_obj_tree_walk(screen, desktopCountUiObjects, &snapshot);
    return snapshot.fullScreenChildCount > 0 && snapshot.objectCount >= 40;
}

static DesktopUiSnapshot desktopCaptureUiSnapshot()
{
    LvglLockGuard lock;
    DesktopUiSnapshot snapshot;
    auto* screen = lv_screen_active();
    if (screen != nullptr) {
        lv_obj_update_layout(screen);
        lv_obj_tree_walk(screen, desktopCountUiObjects, &snapshot);
    }
    return snapshot;
}

static lv_obj_t* desktopTopFullscreenRoot()
{
    auto* screen = lv_screen_active();
    if (screen == nullptr) {
        return nullptr;
    }
    lv_obj_update_layout(screen);

    const auto displayWidth = GetHAL()->getDisplayWidth();
    const auto displayHeight = GetHAL()->getDisplayHeight();
    for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(screen)) - 1; i >= 0; --i) {
        auto* child = lv_obj_get_child(screen, static_cast<uint32_t>(i));
        if (child != nullptr && lv_obj_get_width(child) >= displayWidth && lv_obj_get_height(child) >= displayHeight) {
            return child;
        }
    }
    return nullptr;
}

struct DesktopBoundsValidation {
    const char* context = "";
    bool valid = true;
};

static const char* desktopObjectKind(lv_obj_t* obj)
{
    if (obj == nullptr) {
        return "null";
    }
    if (lv_obj_check_type(obj, &lv_label_class)) {
        return "label";
    }
    if (lv_obj_check_type(obj, &lv_button_class)) {
        return "button";
    }
    if (lv_obj_check_type(obj, &lv_slider_class)) {
        return "slider";
    }
    if (lv_obj_check_type(obj, &lv_switch_class)) {
        return "switch";
    }
    if (lv_obj_check_type(obj, &lv_bar_class)) {
        return "bar";
    }
    if (lv_obj_check_type(obj, &lv_textarea_class)) {
        return "textarea";
    }
    return "object";
}

static void desktopValidateBoundsRecursive(lv_obj_t* parent, DesktopBoundsValidation& validation)
{
    if (parent == nullptr || !validation.valid) {
        return;
    }

    const bool parentScrollable = lv_obj_has_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    const int32_t parentWidth = lv_obj_get_width(parent);
    const int32_t parentHeight = lv_obj_get_height(parent);
    const uint32_t childCount = lv_obj_get_child_count(parent);
    lv_area_t parentArea;
    lv_obj_get_coords(parent, &parentArea);

    for (uint32_t i = 0; i < childCount; ++i) {
        auto* child = lv_obj_get_child(parent, i);
        if (child == nullptr || lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }

        if (!parentScrollable) {
            lv_area_t childArea;
            lv_obj_get_coords(child, &childArea);
            const int32_t x = childArea.x1;
            const int32_t y = childArea.y1;
            const int32_t w = lv_obj_get_width(child);
            const int32_t h = lv_obj_get_height(child);
            if (x < parentArea.x1 || y < parentArea.y1 || w <= 0 || h <= 0 ||
                childArea.x2 > parentArea.x2 || childArea.y2 > parentArea.y2) {
                mclog::tagError(_tag,
                                "desktop UI bounds failed in {}: {} child {} at {},{} {}x{} outside {} parent at {},{} "
                                "{}x{}",
                                validation.context, desktopObjectKind(child), static_cast<unsigned>(i),
                                static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h),
                                desktopObjectKind(parent), static_cast<int>(parentArea.x1),
                                static_cast<int>(parentArea.y1), static_cast<int>(parentWidth),
                                static_cast<int>(parentHeight));
                validation.valid = false;
                return;
            }
        }

        desktopValidateBoundsRecursive(child, validation);
        if (!validation.valid) {
            return;
        }
    }
}

static bool desktopValidateNonScrollableBounds(const char* context)
{
    LvglLockGuard lock;
    auto* topRoot = desktopTopFullscreenRoot();
    if (topRoot == nullptr) {
        return false;
    }

    lv_obj_update_layout(topRoot);
    lv_area_t rootArea;
    lv_obj_get_coords(topRoot, &rootArea);
    if (rootArea.x1 != 0 || rootArea.y1 != 0 || rootArea.x2 != GetHAL()->getDisplayWidth() - 1 ||
        rootArea.y2 != GetHAL()->getDisplayHeight() - 1) {
        return false;
    }
    DesktopBoundsValidation validation;
    validation.context = context;
    desktopValidateBoundsRecursive(topRoot, validation);
    return validation.valid;
}

static bool desktopTopFullscreenRootHasContent()
{
    LvglLockGuard lock;
    auto* topRoot = desktopTopFullscreenRoot();
    if (topRoot == nullptr || lv_obj_get_child_count(topRoot) != 2) {
        return false;
    }

    auto* content = lv_obj_get_child(topRoot, 1);
    if (content == nullptr || lv_obj_get_child_count(content) == 0) {
        return false;
    }

    DesktopUiSnapshot rootSnapshot;
    lv_obj_tree_walk(topRoot, desktopCountUiObjects, &rootSnapshot);
    return rootSnapshot.objectCount >= 8;
}

static void desktopAssertLauncherUiReady()
{
    if (!_desktopAssertLauncherUi || _desktopLauncherUiAsserted) {
        return;
    }

    if (!desktopLauncherUiLooksReady() || !desktopValidateNonScrollableBounds("Home")) {
        mclog::tagError(_tag, "desktop launcher UI smoke assertion failed");
        std::abort();
    }

    _desktopLauncherUiAsserted = true;
    mclog::tagInfo(_tag, "desktop launcher UI smoke assertion passed");
}

static void desktopCollectTextFields(lv_obj_t* object, std::vector<lv_obj_t*>& fields)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if (lv_obj_check_type(object, &lv_textarea_class)) {
        fields.push_back(object);
        return;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        desktopCollectTextFields(lv_obj_get_child(object, i), fields);
    }
}

static lv_obj_t* desktopFindKeyboard(lv_obj_t* object)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
        return nullptr;
    }
    if (lv_obj_check_type(object, &lv_keyboard_class)) {
        return object;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* keyboard = desktopFindKeyboard(lv_obj_get_child(object, i))) {
            return keyboard;
        }
    }
    return nullptr;
}

static bool desktopHasLabel(lv_obj_t* object, const char* text)
{
    if (lv_obj_check_type(object, &lv_label_class) && std::string(lv_label_get_text(object)) == text) {
        return true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (desktopHasLabel(lv_obj_get_child(object, i), text)) {
            return true;
        }
    }
    return false;
}

static bool desktopHasLabelContaining(lv_obj_t* object, const char* text)
{
    if (lv_obj_check_type(object, &lv_label_class) &&
        std::string(lv_label_get_text(object)).find(text) != std::string::npos) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (desktopHasLabelContaining(lv_obj_get_child(object, i), text)) return true;
    }
    return false;
}

static void desktopPressKeyboardKey(lv_obj_t* keyboard, const char* key)
{
    // Exercise LVGL's own keyboard handler, including its nested Ready/Cancel events.
    for (uint32_t i = 0; i < 100; ++i) {
        const char* text = lv_buttonmatrix_get_button_text(keyboard, i);
        if (text == nullptr) {
            break;
        }
        if (std::string(text) == key) {
            lv_buttonmatrix_set_selected_button(keyboard, i);
            lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, nullptr);
            return;
        }
    }
    mclog::tagError(_tag, "desktop touch editor is missing a required keyboard key");
    std::abort();
}

static void desktopSaveUiCapture(const std::string& name)
{
    const char* directory = std::getenv("M5TAB5_DESKTOP_UI_CAPTURE_DIR");
    if (directory == nullptr || directory[0] == '\0') {
        return;
    }
    auto* display = lv_display_get_default();
    // Capture settled visual states rather than the first frame of theme transitions.
    lv_timer_handler();
    SDL_Delay(LV_THEME_DEFAULT_TRANSITION_TIME + 80);
    lv_timer_handler();
    lv_refr_now(display);
    // Read a complete LVGL render, not SDL's undefined back buffer after RenderPresent.
    auto* surface = SDL_CreateRGBSurfaceWithFormat(0, GetHAL()->getDisplayWidth(), GetHAL()->getDisplayHeight(),
                                                 32, SDL_PIXELFORMAT_ARGB8888);
    // Keep a full-screen capture outside LVGL's intentionally small embedded allocator.
    lv_draw_buf_t snapshot{};
    const bool rendered = surface &&
        lv_draw_buf_init(&snapshot, surface->w, surface->h, LV_COLOR_FORMAT_ARGB8888, surface->pitch,
                         surface->pixels, surface->pitch * surface->h) == LV_RESULT_OK &&
        lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888, &snapshot) == LV_RESULT_OK;
    const auto path = (std::filesystem::path(directory) / (name + ".bmp")).string();
    const bool saved = rendered && SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    if (!saved) {
        mclog::tagError(_tag, "desktop UI capture failed: {}", SDL_GetError());
        std::abort();
    }
}

static void desktopAssertTouchTextEditing(const char* context)
{
    LvglLockGuard lock;
    std::vector<lv_obj_t*> fields;
    desktopCollectTextFields(desktopTopFullscreenRoot(), fields);
    for (auto* field : fields) {
        const std::string original = lv_textarea_get_text(field);
        lv_textarea_set_text(field, "seed");
        lv_obj_send_event(field, LV_EVENT_CLICKED, nullptr);
        auto* keyboard = desktopFindKeyboard(desktopTopFullscreenRoot());
        if (keyboard == nullptr) {
            mclog::tagError(_tag, "desktop touch editor missing in {}", context);
            std::abort();
        }
        auto* draft = lv_keyboard_get_textarea(keyboard);
        auto* root = desktopTopFullscreenRoot();
        lv_obj_update_layout(root);
        DesktopBoundsValidation editorBounds;
        editorBounds.context = context;
        desktopValidateBoundsRecursive(root, editorBounds);
        if (draft == nullptr || draft == field || std::string(lv_textarea_get_text(draft)) != "seed" ||
            lv_textarea_get_password_mode(draft) != lv_textarea_get_password_mode(field) ||
            !editorBounds.valid) {
            mclog::tagError(_tag, "desktop touch editor draft/bounds failed in {}", context);
            std::abort();
        }
        desktopPressKeyboardKey(keyboard, "a");
        desktopPressKeyboardKey(keyboard, LV_SYMBOL_OK);
        if (std::string(lv_textarea_get_text(field)) != "seeda" ||
            desktopFindKeyboard(desktopTopFullscreenRoot()) != nullptr) {
            mclog::tagError(_tag, "desktop touch editor save failed in {}", context);
            std::abort();
        }
        lv_obj_send_event(field, LV_EVENT_CLICKED, nullptr);
        keyboard = desktopFindKeyboard(desktopTopFullscreenRoot());
        if (keyboard == nullptr) {
            std::abort();
        }
        desktopPressKeyboardKey(keyboard, "a");
        desktopPressKeyboardKey(keyboard, LV_SYMBOL_KEYBOARD);
        if (std::string(lv_textarea_get_text(field)) != "seeda" ||
            desktopFindKeyboard(desktopTopFullscreenRoot()) != nullptr) {
            mclog::tagError(_tag, "desktop touch editor cancel failed in {}", context);
            std::abort();
        }
        lv_obj_send_event(field, LV_EVENT_CLICKED, nullptr);
        keyboard = desktopFindKeyboard(desktopTopFullscreenRoot());
        desktopPressKeyboardKey(keyboard, LV_SYMBOL_NEW_LINE);
        if (desktopFindKeyboard(desktopTopFullscreenRoot()) != nullptr) {
            mclog::tagError(_tag, "desktop touch editor Enter failed in {}", context);
            std::abort();
        }
        lv_textarea_set_text(field, original.c_str());
    }
    // Leave the editor open to test app-close cleanup as well.
    if (!fields.empty()) {
        lv_obj_send_event(fields.back(), LV_EVENT_CLICKED, nullptr);
        desktopSaveUiCapture(std::string(context) + "-editor");
    }
}

static lv_obj_t* desktopFindButton(lv_obj_t* object, const char* label)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
        return nullptr;
    }
    if (lv_obj_check_type(object, &lv_button_class) && desktopHasLabel(object, label)) {
        return object;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* button = desktopFindButton(lv_obj_get_child(object, i), label)) {
            return button;
        }
    }
    return nullptr;
}

// Exercise LVGL's pointer target resolution, including clipping and overlapping siblings.
static void desktopAssertHitTarget(lv_obj_t* target)
{
    auto* root = desktopTopFullscreenRoot();
    lv_obj_update_layout(root);
    lv_area_t area;
    lv_obj_get_coords(target, &area);
    lv_point_t point{(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    auto* hit = lv_indev_search_obj(lv_screen_active(), &point);
    while (hit && hit != target) hit = lv_obj_get_parent(hit);
    if (!hit) {
        mclog::tagError(_tag, "desktop pointer target is obscured or clipped");
        std::abort();
    }
}

static void desktopWaitForWifiState(hal::HalBase::WifiConnectionState state, const char* label)
{
    const uint32_t started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 2000) {
        desktopPumpLifecycle();
        if (GetHAL()->wifiGetStatus().state == state && !app_runtime::globalAsyncJobRunner().busy()) {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            std::vector<lv_obj_t*> fields;
            desktopCollectTextFields(root, fields);
            if (desktopHasLabel(root, label) && fields.size() == 2 &&
                !lv_obj_has_state(fields[0], LV_STATE_DISABLED)) {
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    mclog::tagError(_tag, "desktop Wi-Fi workflow did not reach {}", label);
    std::abort();
}

static void desktopClickNetworkAction(const char* label)
{
    LvglLockGuard lock;
    auto* button = desktopFindButton(desktopTopFullscreenRoot(), label);
    if (button == nullptr || lv_obj_has_state(button, LV_STATE_DISABLED)) {
        mclog::tagError(_tag, "desktop network action {} must be reachable", label);
        std::abort();
    }
    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}

static void desktopAssertNetworkSelection()
{
    {
        LvglLockGuard lock;
        auto* scan = desktopFindButton(desktopTopFullscreenRoot(), "Scan");
        if (scan == nullptr) {
            std::abort();
        }
        lv_obj_send_event(scan, LV_EVENT_CLICKED, nullptr);
        if (!lv_obj_has_state(scan, LV_STATE_DISABLED)) {
            mclog::tagError(_tag, "desktop Scan must be disabled while a scan is running");
            std::abort();
        }
    }
    const uint32_t started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 2000) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            if (desktopEnvEnabled("M5TAB5_DESKTOP_WIFI_NO_NETWORKS") && desktopHasLabel(root, "No networks found")) {
                auto* scan = desktopFindButton(root, "Scan");
                if (lv_obj_has_state(scan, LV_STATE_DISABLED)) {
                    std::abort();
                }
                desktopSaveUiCapture("Network-empty");
                return;
            }
            if (auto* workshop = desktopFindButton(root, "Workshop")) {
                std::vector<lv_obj_t*> fields;
                desktopCollectTextFields(root, fields);
                if (fields.size() != 2) {
                    std::abort();
                }
                if (desktopEnvEnabled("M5TAB5_DESKTOP_WIFI_MANY_NETWORKS")) {
                    auto* last = desktopFindButton(root, "01234567890123456789012345678901");
                    auto* scan = desktopFindButton(root, "Scan");
                    auto* connect = desktopFindButton(root, "Connect");
                    if (!last || !scan || !connect) std::abort();
                    lv_obj_update_layout(root);
                    lv_area_t scanBefore, connectBefore, scanAfter, connectAfter;
                    lv_obj_get_coords(scan, &scanBefore);
                    lv_obj_get_coords(connect, &connectBefore);
                    lv_obj_scroll_to_view(last, LV_ANIM_OFF);
                    lv_obj_update_layout(root);
                    lv_obj_get_coords(scan, &scanAfter);
                    lv_obj_get_coords(connect, &connectAfter);
                    if (scanBefore.y1 != scanAfter.y1 || connectBefore.y1 != connectAfter.y1) std::abort();
                    desktopAssertHitTarget(last);
                    desktopAssertHitTarget(scan);
                    desktopAssertHitTarget(connect);
                    lv_textarea_set_text(fields[1], "old-password");
                    lv_obj_send_event(last, LV_EVENT_CLICKED, nullptr);
                    if (std::string(lv_textarea_get_text(fields[0])) != "01234567890123456789012345678901" ||
                        lv_textarea_get_text(fields[1])[0] != '\0') std::abort();
                    desktopSaveUiCapture("Network-populated-last");
                    lv_obj_scroll_to_view(workshop, LV_ANIM_OFF);
                }
                lv_textarea_set_text(fields[1], "previous-password");
                desktopAssertHitTarget(workshop);
                lv_obj_send_event(workshop, LV_EVENT_CLICKED, nullptr);
                if (std::string(lv_textarea_get_text(fields[0])) != "Workshop" ||
                    lv_textarea_get_text(fields[1])[0] != '\0') {
                    mclog::tagError(_tag, "desktop network selection must fill SSID and clear the old password");
                    std::abort();
                }
                desktopSaveUiCapture("Network-selected");
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    {
        LvglLockGuard lock;
        std::vector<lv_obj_t*> fields;
        desktopCollectTextFields(desktopTopFullscreenRoot(), fields);
        if (fields.size() != 2 || std::string(lv_textarea_get_text(fields[0])) != "Workshop") {
            mclog::tagError(_tag, "desktop Wi-Fi scan must render selectable network rows");
            std::abort();
        }
        lv_textarea_set_text(fields[1], "simulated-password");
        lv_obj_send_event(desktopFindButton(desktopTopFullscreenRoot(), "Workshop"), LV_EVENT_CLICKED, nullptr);
        if (std::string(lv_textarea_get_text(fields[1])) != "simulated-password") {
            mclog::tagError(_tag, "desktop selecting the same network must preserve its password");
            std::abort();
        }
    }
    desktopClickNetworkAction("Connect");
    if (desktopEnvEnabled("M5TAB5_DESKTOP_WIFI_CONNECT_FAIL")) {
        desktopWaitForWifiState(hal::HalBase::WifiConnectionState::Failed, "Failed");
        {
            LvglLockGuard lock;
            if (!desktopHasLabel(desktopTopFullscreenRoot(), "Simulated connection failure")) {
                std::abort();
            }
            desktopSaveUiCapture("Network-failure");
        }
        desktopClickNetworkAction("Forget");
    } else {
        desktopWaitForWifiState(hal::HalBase::WifiConnectionState::Connected, "Connected");
        if (GetHAL()->wifiGetStatus().ssid != "Workshop" || GetHAL()->wifiGetStatus().ipAddress.empty()) {
            std::abort();
        }
        desktopClickNetworkAction("Scan");
        desktopWaitForWifiState(hal::HalBase::WifiConnectionState::Connected, "Connected");
        if (GetHAL()->wifiGetStatus().ssid != "Workshop") {
            std::abort();
        }
        desktopClickNetworkAction("Disconnect");
    }
    desktopWaitForWifiState(hal::HalBase::WifiConnectionState::Idle, "Idle");
    desktopClickNetworkAction("Start AP");
    desktopWaitForWifiState(hal::HalBase::WifiConnectionState::ApRunning, "AP running");
    {
        LvglLockGuard lock;
        auto* scan = desktopFindButton(desktopTopFullscreenRoot(), "Scan");
        if (!lv_obj_has_state(scan, LV_STATE_DISABLED)) {
            std::abort();
        }
    }
    desktopClickNetworkAction("Stop AP");
    desktopWaitForWifiState(hal::HalBase::WifiConnectionState::Idle, "Idle");
}

static void desktopAssertRtcWorkflow()
{
    auto check = [](bool ok, const char* reason) {
        if (!ok) { mclog::tagError(_tag, "desktop RTC workflow: {}", reason); std::abort(); }
    };
    auto waitFor = [](const char* text) {
        const auto started = GetHAL()->millis();
        while (GetHAL()->millis() - started < 3000) {
            desktopPumpLifecycle();
            {
                LvglLockGuard lock;
                auto* root = desktopTopFullscreenRoot();
                auto* button = desktopFindButton(root, "Set clock");
                if (desktopHasLabelContaining(root, text) && button &&
                    !lv_obj_has_state(button, LV_STATE_DISABLED)) return;
            }
            SDL_Delay(5);
        }
        mclog::tagError(_tag, "desktop RTC workflow timed out: {}", text);
        std::abort();
    };
    if (desktopEnvEnabled("M5TAB5_DESKTOP_RTC_FAIL")) {
        waitFor("Simulated RTC read failure");
        desktopSaveUiCapture("RTC-read-failed");
        return;
    }
    const bool initialize = desktopEnvEnabled("M5TAB5_DESKTOP_RTC_INVALID");
    waitFor(initialize ? "initialization required" : "Simulated clock");
    {
        LvglLockGuard lock;
        std::vector<lv_obj_t*> fields;
        auto* root = desktopTopFullscreenRoot();
        desktopCollectTextFields(root, fields);
        check(fields.size() == 2, "expected date and time fields");
        lv_textarea_set_text(fields[0], "2026-02-30");
        lv_textarea_set_text(fields[1], "12:34:56");
        desktopClickNetworkAction("Set clock");
        check(desktopHasLabelContaining(root, "Invalid date/time") && !desktopFindButton(root, "Confirm"), "invalid calendar reached confirmation");
        lv_textarea_set_text(fields[0], "2028-02-29");
        desktopClickNetworkAction("Set clock");
        // LVGL's fitted labels restore their full text after deferred layout.
        lv_obj_update_layout(root);
        check(desktopHasLabel(root, initialize ? "Initialize RTC and set time?" : "Set clock?"), "valid calendar did not reach confirmation");
        desktopSaveUiCapture("RTC-confirmation");
        desktopClickNetworkAction("Cancel");
        check(!desktopFindButton(root, "Confirm"), "Cancel left confirmation visible");
        desktopClickNetworkAction("Set clock");
        desktopClickNetworkAction("Confirm");
    }
    waitFor("Clock set and readback verified");
    const auto result = GetHAL()->getRtcData();
    check(result.valid && !result.needsInitialization && result.time.tm_year == 128 &&
        result.time.tm_mon == 1 && result.time.tm_mday == 29 && result.time.tm_hour == 12 &&
        result.time.tm_min == 34, "verified result differs from requested calendar");
    if (!desktopValidateNonScrollableBounds("RTC setting")) std::abort();
    desktopSaveUiCapture("RTC-set-verified");
}

static void desktopAssertUpdatesWorkflow(int appId)
{
    using State = hal::HalBase::OtaState;
    auto check = [](bool ok, const char* reason) {
        if (!ok) { mclog::tagError(_tag, "desktop Updates workflow: {}", reason); std::abort(); }
    };
    auto waitFor = [&](State expected, const char* enabledButton) {
        const auto started = GetHAL()->millis();
        while (GetHAL()->millis() - started < 5000) {
            desktopPumpLifecycle();
            LvglLockGuard lock;
            auto* button = desktopFindButton(desktopTopFullscreenRoot(), enabledButton);
            if (GetHAL()->otaGetStatus().state == expected && button &&
                !lv_obj_has_state(button, LV_STATE_DISABLED)) return;
            SDL_Delay(5);
        }
        check(false, "state/control did not settle");
    };
    if (!GetHAL()->wifiConnect("Workshop", "simulated-password")) {
        desktopPumpLifecycle();
        check(lv_obj_has_state(desktopFindButton(desktopTopFullscreenRoot(), "Install Update"), LV_STATE_DISABLED),
              "Install enabled without Wi-Fi");
        return;
    }
    waitFor(State::Idle, "Install Update");
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        std::vector<lv_obj_t*> fields;
        desktopCollectTextFields(root, fields);
        check(fields.size() == 1, "expected URL input");
        lv_textarea_set_text(fields[0], "http://example.com/firmware.bin");
        desktopClickNetworkAction("Install Update");
        check(desktopHasLabelContaining(root, "HTTPS") && !desktopFindButton(root, "Confirm"), "HTTP URL accepted");
        lv_textarea_set_text(fields[0], "https://example.com/m5stack_tab5.bin");
        desktopClickNetworkAction("Install Update");
        lv_obj_update_layout(root);
        check(desktopHasLabel(root, "Install firmware update?"), "missing install confirmation");
        check(GetHAL()->otaGetStatus().state == State::Idle, "download started before confirmation");
        desktopSaveUiCapture("Updates-confirmation");
        desktopClickNetworkAction("Cancel");
        check(GetHAL()->otaGetStatus().state == State::Idle, "cancelled dialog started a transfer");
        desktopClickNetworkAction("Install Update");
        desktopClickNetworkAction("Confirm");
        check(lv_obj_has_state(fields[0], LV_STATE_DISABLED), "URL remained editable during update");
    }
    // A screen may be destroyed while the registered transfer retains ownership.
    desktopClickNetworkAction("Back");
    desktopPumpLifecycle();
    check(GetMooncake().getAppCurrentState(appId) == AppAbility::StateSleeping, "Back did not close Updates");
    check(app_registry::registry().open(app_registry::AppKey::Updates), "cannot reopen Updates");
    desktopPumpLifecycle();
    {
        LvglLockGuard lock;
        desktopClickNetworkAction("Cancel Update");
    }
    waitFor(State::Cancelled, "Install Update");
    check(!GetHAL()->otaGetStatus().rebootRequired, "cancelled update selected boot image");
    desktopSaveUiCapture("Updates-cancelled");
    {
        LvglLockGuard lock;
        std::vector<lv_obj_t*> fields;
        desktopCollectTextFields(desktopTopFullscreenRoot(), fields);
        check(fields.size() == 1, "URL field missing after reopen");
        lv_textarea_set_text(fields[0], "https://example.com/m5stack_tab5.bin");
        desktopClickNetworkAction("Install Update");
        desktopClickNetworkAction("Confirm");
    }
    if (desktopEnvEnabled("M5TAB5_DESKTOP_OTA_FAIL")) {
        waitFor(State::Failed, "Install Update");
        check(!GetHAL()->otaGetStatus().rebootRequired, "failed update requires reboot");
        desktopSaveUiCapture("Updates-failed");
    } else {
        waitFor(State::Complete, "Reboot");
        check(GetHAL()->otaGetStatus().simulated, "desktop update was not identified as simulation");
        desktopSaveUiCapture("Updates-complete");
        desktopClickNetworkAction("Reboot");
        desktopClickNetworkAction("Cancel");
        check(GetHAL()->otaGetStatus().rebootRequired, "reboot ran before confirmation");
        desktopClickNetworkAction("Reboot");
        desktopClickNetworkAction("Confirm");
        waitFor(State::Idle, "Install Update");
    }
    check(desktopValidateNonScrollableBounds("Updates workflow"), "off-screen update controls");
    GetHAL()->wifiDisconnect();
}

static size_t desktopTimerCount()
{
    size_t count = 0;
    for (auto* timer = lv_timer_get_next(nullptr); timer != nullptr; timer = lv_timer_get_next(timer)) {
        ++count;
    }
    return count;
}

static lv_obj_t* desktopFindCanvas(lv_obj_t* object)
{
    if (lv_obj_check_type(object, &lv_canvas_class)) {
        return object;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* canvas = desktopFindCanvas(lv_obj_get_child(object, i))) {
            return canvas;
        }
    }
    return nullptr;
}

static void desktopClickCameraAction(const char* text)
{
    LvglLockGuard lock;
    auto* button = desktopFindButton(desktopTopFullscreenRoot(), text);
    if (button == nullptr || lv_obj_has_state(button, LV_STATE_DISABLED)) {
        mclog::tagError(_tag, "desktop camera action unavailable: {}", text);
        std::abort();
    }
    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}

static void desktopWaitForCameraState(hal::HalBase::CameraState state, const char* label)
{
    const uint32_t started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 3000) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            lv_timer_handler();
            const auto snapshot = GetHAL()->cameraGetStatus();
            if (snapshot.state == state && desktopHasLabel(desktopTopFullscreenRoot(), label) &&
                (state != hal::HalBase::CameraState::Running || snapshot.frames >= 3)) {
                return;
            }
        }
        SDL_Delay(5);
    }
    mclog::tagError(_tag, "desktop camera state timed out: {}", label);
    std::abort();
}

static void desktopAssertCameraWorkflow()
{
    using State = hal::HalBase::CameraState;
    size_t baselineTimers;
    {
        LvglLockGuard lock;
        baselineTimers = desktopTimerCount();
        auto* stop = desktopFindButton(desktopTopFullscreenRoot(), "Stop");
        if (stop == nullptr || !lv_obj_has_state(stop, LV_STATE_DISABLED)) {
            std::abort();
        }
    }
    const char* fault = std::getenv("M5TAB5_DESKTOP_CAMERA_FAIL");
    const bool expectFailure = fault != nullptr && fault[0] != '\0';
    desktopClickCameraAction("Start");
    if (expectFailure) {
        desktopWaitForCameraState(State::Failed, "Camera failed");
        {
            LvglLockGuard lock;
            const auto status = GetHAL()->cameraGetStatus();
            if (status.lastError.empty() || !desktopHasLabel(desktopTopFullscreenRoot(), status.lastError.c_str()) ||
                desktopTimerCount() != baselineTimers) {
                std::abort();
            }
            desktopSaveUiCapture("Camera-failure");
        }
        unsetenv("M5TAB5_DESKTOP_CAMERA_FAIL");
        desktopClickCameraAction("Start");
    }
    for (int cycle = 0; cycle < 5; ++cycle) {
        desktopWaitForCameraState(State::Running, "Camera running");
        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            auto* canvas = desktopFindCanvas(root);
            auto* buffer = canvas == nullptr ? nullptr : lv_canvas_get_draw_buf(canvas);
            const auto status = GetHAL()->cameraGetStatus();
            auto* start = desktopFindButton(root, "Start");
            if (buffer == nullptr || buffer->header.w != status.width || buffer->header.h != status.height ||
                status.width != lv_obj_get_width(canvas) || status.height != lv_obj_get_height(canvas) ||
                !lv_obj_has_state(start, LV_STATE_DISABLED) || desktopTimerCount() != baselineTimers + 1) {
                std::abort();
            }
            auto* pixels = reinterpret_cast<uint16_t*>(buffer->data);
            bool varied = false;
            for (uint32_t x = 1; x < buffer->header.w; ++x) {
                varied = varied || pixels[x] != pixels[0];
            }
            if (!varied) {
                mclog::tagError(_tag, "desktop camera preview is blank");
                std::abort();
            }
            if (cycle == 0) {
                desktopSaveUiCapture("Camera-running");
            }
        }
        if (!desktopValidateNonScrollableBounds("Camera running")) {
            std::abort();
        }
        desktopClickCameraAction("Stop");
        desktopWaitForCameraState(State::Stopped, "Camera stopped");
        {
            LvglLockGuard lock;
            auto* canvas = desktopFindCanvas(desktopTopFullscreenRoot());
            if (lv_image_get_src(canvas) != nullptr || desktopTimerCount() != baselineTimers) {
                std::abort();
            }
        }
        desktopClickCameraAction("Start");
    }
    // Leave capture active so the normal Back/cleanup assertions exercise close during capture.
    desktopWaitForCameraState(State::Running, "Camera running");
}

static lv_obj_t* desktopFindSwitch(lv_obj_t* object)
{
    if (lv_obj_check_type(object, &lv_switch_class)) {
        return object;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* toggle = desktopFindSwitch(lv_obj_get_child(object, i))) {
            return toggle;
        }
    }
    return nullptr;
}

static void desktopWaitForPpsLabel(const char* label)
{
    const auto started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 3000) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            if (desktopHasLabel(root, label)) {
                // Output-off readback can arrive before the rest of the telemetry or command completion.
                const bool readyOff = std::string(label) != "Output Off confirmed" ||
                    (!lv_obj_has_state(desktopFindButton(root, "Apply"), LV_STATE_DISABLED) &&
                     desktopHasLabel(root, "0.000 A"));
                const bool readyOn = std::string(label) != "Output On" ||
                    desktopHasLabel(root, "Mode: CV") || desktopHasLabel(root, "Mode: CC");
                if (readyOff && readyOn) {
                    return;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    mclog::tagError(_tag, "desktop PPS did not reach {}", label);
    std::abort();
}

static void desktopSetPpsOutput(bool enabled)
{
    LvglLockGuard lock;
    auto* toggle = desktopFindSwitch(desktopTopFullscreenRoot());
    if (toggle == nullptr || lv_obj_has_state(toggle, LV_STATE_DISABLED)) {
        mclog::tagError(_tag, "desktop PPS output toggle unavailable");
        std::abort();
    }
    if (enabled) {
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(toggle, LV_STATE_CHECKED);
    }
    lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
}

[[noreturn]] static void desktopPpsFailure(int line)
{
    mclog::tagError(_tag, "PPS workflow failed at source line {}", line);
    std::abort();
}

static void desktopAssertPpsWorkflow()
{
    if (desktopEnvEnabled("M5TAB5_DESKTOP_PPS_ABSENT")) {
        desktopWaitForPpsLabel("No PPS detected / check 9-36V input power");
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        if (!desktopHasLabel(root, "Output unknown - may be live") ||
            !desktopHasLabel(root, "-- A") ||
            !lv_obj_has_state(desktopFindButton(root, "Apply"), LV_STATE_DISABLED) ||
            !lv_obj_has_state(desktopFindSwitch(root), LV_STATE_DISABLED) ||
            lv_obj_has_state(desktopFindButton(root, "Emergency Off"), LV_STATE_DISABLED)) {
            desktopPpsFailure(__LINE__);
        }
        desktopSaveUiCapture("PPS-absent");
        return;
    }
    desktopWaitForPpsLabel("Output Off confirmed");
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        if (!desktopHasLabel(root, "0.000 A") || !desktopHasLabel(root, "Set V: 5.000 V")) {
            mclog::tagError(_tag, "desktop PPS initial telemetry not ready");
            desktopPpsFailure(__LINE__);
        }
        lv_obj_send_event(desktopFindButton(root, "12V"), LV_EVENT_CLICKED, nullptr);
        if (!lv_obj_has_state(desktopFindSwitch(root), LV_STATE_DISABLED)) {
            mclog::tagError(_tag, "desktop PPS preset must lock Enable until Apply");
            desktopPpsFailure(__LINE__);
        }
        std::vector<lv_obj_t*> fields;
        desktopCollectTextFields(root, fields);
        if (fields.size() != 2) desktopPpsFailure(__LINE__);
        lv_textarea_set_text(fields[0], "30.001");
        if (!lv_obj_has_state(desktopFindButton(root, "Apply"), LV_STATE_DISABLED) ||
            !lv_obj_has_state(desktopFindSwitch(root), LV_STATE_DISABLED)) desktopPpsFailure(__LINE__);
        lv_obj_send_event(fields[0], LV_EVENT_CLICKED, nullptr);
        auto* keyboard = desktopFindKeyboard(root);
        if (!keyboard) desktopPpsFailure(__LINE__);
        lv_textarea_set_text(lv_keyboard_get_textarea(keyboard), "12.345");
        desktopPressKeyboardKey(keyboard, LV_SYMBOL_OK);
        lv_textarea_set_text(fields[1], "2.123");
        if (!desktopHasLabel(root, "Set V: 12.345 V") || !desktopHasLabel(root, "Set A: 2.123 A")) desktopPpsFailure(__LINE__);
        desktopSaveUiCapture("PPS-exact-entry");
        lv_obj_send_event(desktopFindButton(root, "Apply"), LV_EVENT_CLICKED, nullptr);
    }
    const auto started = GetHAL()->millis();
    bool applied = false;
    while (GetHAL()->millis() - started < 3000 && !applied) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            applied = !lv_obj_has_state(desktopFindSwitch(root), LV_STATE_DISABLED) &&
                      desktopHasLabel(root, "Set V: 12.345 V");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!applied) {
        mclog::tagError(_tag, "desktop PPS Apply did not finish");
        desktopPpsFailure(__LINE__);
    }
    uint8_t voltageBytes[4];
    float voltage = 0;
    if (!GetHAL()->i2cReadRegister(hal::ModuleBus::MBus, 0x35, 0x18, voltageBytes, sizeof(voltageBytes))) {
        desktopPpsFailure(__LINE__);
    }
    std::memcpy(&voltage, voltageBytes, sizeof(voltage));
    if (voltage != 12.345f) {
        mclog::tagError(_tag, "desktop PPS setpoint readback mismatch");
        desktopPpsFailure(__LINE__);
    }
    if (!GetHAL()->i2cReadRegister(hal::ModuleBus::MBus, 0x35, 0x1C, voltageBytes, sizeof(voltageBytes))) desktopPpsFailure(__LINE__);
    std::memcpy(&voltage, voltageBytes, sizeof(voltage));
    if (voltage != 2.123f) desktopPpsFailure(__LINE__);
    desktopSetPpsOutput(true);
    desktopWaitForPpsLabel("Output On");
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        if (!lv_obj_has_state(desktopFindButton(root, "Apply"), LV_STATE_DISABLED)) {
            mclog::tagError(_tag, "desktop PPS Apply must be locked while output is on");
            desktopPpsFailure(__LINE__);
        }
        desktopSaveUiCapture("PPS-running");
        std::vector<lv_obj_t*> fields;
        desktopCollectTextFields(root, fields);
        lv_obj_send_event(fields[0], LV_EVENT_CLICKED, nullptr);
        if (!desktopFindKeyboard(root)) desktopPpsFailure(__LINE__);
        auto* off = desktopFindButton(root, "Emergency Off");
        if (!off || lv_obj_has_state(off, LV_STATE_DISABLED)) desktopPpsFailure(__LINE__);
        desktopSaveUiCapture("PPS-editor-output-on");
        desktopAssertHitTarget(off);
        lv_obj_send_event(off, LV_EVENT_CLICKED, nullptr);
        if (desktopFindKeyboard(root)) desktopPpsFailure(__LINE__);
    }
    desktopWaitForPpsLabel("Output Off confirmed");
    if (!desktopValidateNonScrollableBounds("PPS workflow")) {
        desktopPpsFailure(__LINE__);
    }
    // Leave output on so Back exercises asynchronous output-off, not merely object deletion.
    desktopSetPpsOutput(true);
    desktopWaitForPpsLabel("Output On");
}

static void desktopWaitPowerLabel(const char* text)
{
    const auto started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 3000) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            lv_timer_handler();
            if (desktopHasLabel(desktopTopFullscreenRoot(), text)) return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    mclog::tagError(_tag, "desktop power state did not arrive: {}", text);
    std::abort();
}

static void desktopPowerClick(const char* text)
{
    LvglLockGuard lock;
    auto* button = desktopFindButton(desktopTopFullscreenRoot(), text);
    if (!button || lv_obj_has_state(button, LV_STATE_DISABLED)) {
        mclog::tagError(_tag, "desktop power action unavailable: {}", text);
        std::abort();
    }
    const auto started = GetHAL()->millis();
    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
    if (GetHAL()->millis() - started > 100) {
        mclog::tagError(_tag, "desktop power action blocked UI: {}", text);
        std::abort();
    }
}

static void desktopAssertPowerWorkflow(int appId)
{
    const auto brightness = GetHAL()->getDisplayBrightness();
    for (const char* action : {"Screen Off", "Motion Sleep", "Timed Sleep", "Power Off"}) {
        desktopPowerClick(action);
        if (!desktopValidateNonScrollableBounds("Power confirmation")) std::abort();
        {
            LvglLockGuard lock;
            if (!desktopHasLabel(desktopTopFullscreenRoot(), (std::string(action) + "?").c_str()) ||
                GetHAL()->isDisplayAsleep() || GetHAL()->getDisplayBrightness() != brightness) {
                mclog::tagError(_tag, "desktop power action ran before confirmation");
                std::abort();
            }
        }
        desktopPowerClick("Cancel");
    }
    desktopPowerClick("Power Off");
    desktopPowerClick("Back");
    desktopPumpLifecycle();
    if (GetMooncake().getAppCurrentState(appId) != AppAbility::StateRunning) {
        mclog::tagError(_tag, "Back must dismiss power confirmation before leaving app");
        std::abort();
    }
    desktopPowerClick("Screen Off");
    desktopPowerClick("Confirm");
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        if (!desktopHasLabel(root, "Preparing") ||
            !lv_obj_has_state(desktopFindButton(root, "Power Off"), LV_STATE_DISABLED)) std::abort();
    }
    if (desktopEnvEnabled("M5TAB5_DESKTOP_POWER_FAIL")) {
        desktopWaitPowerLabel("Simulated power action failure");
        {
            LvglLockGuard lock;
            if (GetHAL()->isDisplayAsleep() || GetHAL()->getDisplayBrightness() != brightness) std::abort();
            desktopSaveUiCapture("Power-error");
        }
        unsetenv("M5TAB5_DESKTOP_POWER_FAIL");
        desktopPowerClick("Screen Off");
        desktopPowerClick("Confirm");
    }
    desktopWaitPowerLabel("Display off (simulated)");
    if (!GetHAL()->isDisplayAsleep() || GetHAL()->getDisplayBrightness() != 0) std::abort();
    desktopPowerClick("Back");
    desktopPumpLifecycle();
    if (GetMooncake().getAppCurrentState(appId) != AppAbility::StateSleeping || !GetHAL()->isDisplayAsleep()) std::abort();
    if (!app_registry::registry().open(app_registry::AppKey::SettingsPower)) std::abort();
    desktopPumpLifecycle();
    desktopWaitPowerLabel("Display off (simulated)");
    desktopPowerClick("Wake Display");
    desktopWaitPowerLabel("Display awake");
    if (GetHAL()->isDisplayAsleep() || GetHAL()->getDisplayBrightness() != brightness) std::abort();
    for (const char* action : {"Motion Sleep", "Timed Sleep", "Power Off"}) {
        desktopPowerClick(action);
        {
            LvglLockGuard lock;
            desktopSaveUiCapture("Power-confirmation");
        }
        desktopPowerClick("Confirm");
        desktopWaitPowerLabel(std::strcmp(action, "Motion Sleep") == 0 ? "Motion sleep simulated" :
                              std::strcmp(action, "Timed Sleep") == 0 ? "Timed sleep simulated: 5 minutes" : "Power off simulated");
    }
    if (!desktopValidateNonScrollableBounds("Power workflow")) std::abort();
    {
        LvglLockGuard lock;
        desktopSaveUiCapture("Power-running");
    }
}

static void desktopCheckGpio(bool condition, const char* message)
{
    if (!condition) {
        mclog::tagError(_tag, "desktop GPIO workflow: {}", message);
        std::abort();
    }
}

static lv_obj_t* desktopGpioCard(lv_obj_t* object, uint8_t pin)
{
    const auto title = "GPIO " + std::to_string(pin);
    if (lv_obj_check_type(object, &lv_label_class) && title == lv_label_get_text(object)) {
        return lv_obj_get_parent(object);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* card = desktopGpioCard(lv_obj_get_child(object, i), pin)) return card;
    }
    return nullptr;
}

static void desktopSeedIndependentGpio()
{
    const char* fault = std::getenv("M5TAB5_DESKTOP_GPIO_FAIL");
    const std::string saved = fault ? fault : "";
    unsetenv("M5TAB5_DESKTOP_GPIO_FAIL");
    desktopCheckGpio(GetHAL()->gpioInitOutput(50) == hal::GpioClaimResult::Claimed &&
                     GetHAL()->gpioSetLevel(50, true), "cannot seed independent output");
    if (!saved.empty()) setenv("M5TAB5_DESKTOP_GPIO_FAIL", saved.c_str(), 1);
}

static void desktopAssertGpioWorkflow(int appId)
{
    const char* faultEnv = std::getenv("M5TAB5_DESKTOP_GPIO_FAIL");
    const std::string fault = faultEnv ? faultEnv : "";
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        for (const auto& info : GetHAL()->gpioGetPins()) {
            const auto state = GetHAL()->gpioGetOutputState(info.pin);
            desktopCheckGpio(info.pin == 50 ? state.claimed && state.levelKnown && state.high : !state.claimed,
                             "opening changed a pin");
        }
        for (uint8_t pin : {0, 1, 53, 54}) {
            auto* card = desktopGpioCard(root, pin);
            auto* action = desktopFindButton(card, "Reserved");
            auto* toggle = desktopFindSwitch(card);
            desktopCheckGpio(card && action && toggle && lv_obj_has_state(action, LV_STATE_DISABLED) &&
                             lv_obj_has_flag(toggle, LV_OBJ_FLAG_HIDDEN), "protected controls are active");
            desktopCheckGpio(GetHAL()->gpioInitOutput(pin) == hal::GpioClaimResult::Rejected &&
                             !GetHAL()->gpioSetLevel(pin, true) && !GetHAL()->gpioReset(pin),
                             "HAL permits protected pin output");
        }
        auto* card = desktopGpioCard(root, 49);
        auto* toggle = desktopFindSwitch(card);
        desktopCheckGpio(desktopHasLabel(card, "Unclaimed") && lv_obj_has_state(toggle, LV_STATE_DISABLED),
                         "unclaimed pin must not have an active toggle");
        lv_obj_send_event(desktopFindButton(card, "Drive Low"), LV_EVENT_CLICKED, nullptr);
        if (fault == "init") {
            desktopCheckGpio(desktopHasLabel(card, "Unknown") && lv_obj_has_state(toggle, LV_STATE_DISABLED) &&
                             GetHAL()->gpioGetOutputState(49).claimed, "partial initialization not retained");
            desktopSaveUiCapture("GPIO-init-error");
            unsetenv("M5TAB5_DESKTOP_GPIO_FAIL");
            lv_obj_send_event(desktopFindButton(card, "Release"), LV_EVENT_CLICKED, nullptr);
            lv_obj_send_event(desktopFindButton(card, "Drive Low"), LV_EVENT_CLICKED, nullptr);
        }
        desktopCheckGpio(desktopHasLabel(card, "Low") && !lv_obj_has_state(toggle, LV_STATE_DISABLED),
                         "explicit claim did not establish low output");
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
        lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
        if (fault == "set") {
            desktopCheckGpio(desktopHasLabel(card, "Unknown") && lv_obj_has_state(toggle, LV_STATE_DISABLED),
                             "failed write must invalidate level");
            desktopSaveUiCapture("GPIO-write-error");
            unsetenv("M5TAB5_DESKTOP_GPIO_FAIL");
            lv_obj_send_event(desktopFindButton(card, "Release"), LV_EVENT_CLICKED, nullptr);
            lv_obj_send_event(desktopFindButton(card, "Drive Low"), LV_EVENT_CLICKED, nullptr);
            lv_obj_add_state(toggle, LV_STATE_CHECKED);
            lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
        }
        desktopCheckGpio(GetHAL()->gpioGetOutputState(49).high && desktopHasLabel(card, "High"),
                         "toggle did not establish high output");
        desktopSaveUiCapture("GPIO-active");
        if (fault == "release") {
            lv_obj_send_event(desktopFindButton(root, "Back"), LV_EVENT_CLICKED, nullptr);
            desktopCheckGpio(desktopHasLabel(card, "Unknown") &&
                             desktopHasLabel(root, "GPIO release failed; a pin may still be driven"),
                             "release failure hidden on Back");
            desktopSaveUiCapture("GPIO-release-error");
        }
    }
    if (fault == "release") {
        desktopPumpLifecycle();
        desktopCheckGpio(GetMooncake().getAppCurrentState(appId) == AppAbility::StateRunning,
                         "Back closed with a failed release");
        unsetenv("M5TAB5_DESKTOP_GPIO_FAIL");
    }
    {
        LvglLockGuard lock;
        auto* root = desktopTopFullscreenRoot();
        auto* card = desktopGpioCard(root, 49);
        lv_obj_send_event(desktopFindButton(card, "Release"), LV_EVENT_CLICKED, nullptr);
        desktopCheckGpio(!GetHAL()->gpioGetOutputState(49).claimed && desktopHasLabel(card, "Unclaimed"),
                         "release did not relinquish output");
        // A rejected claim of another owner's pin must not be released by Release All or Back.
        auto* independent = desktopGpioCard(root, 50);
        lv_obj_send_event(desktopFindButton(independent, "Drive Low"), LV_EVENT_CLICKED, nullptr);
        desktopCheckGpio(desktopHasLabel(independent, "Unclaimed") && GetHAL()->gpioGetOutputState(50).high,
                         "rejected claim altered independent output");
        auto* last = desktopGpioCard(root, 51);
        auto* grid = lv_obj_get_parent(last);
        auto* releaseAll = desktopFindButton(root, "Release All");
        auto* back = desktopFindButton(root, "Back");
        lv_area_t releaseBefore, backBefore;
        lv_obj_get_coords(releaseAll, &releaseBefore);
        lv_obj_get_coords(back, &backBefore);
        lv_obj_scroll_to_view(last, LV_ANIM_OFF);
        lv_obj_update_layout(root);
        lv_area_t cardArea, gridArea, releaseAfter, backAfter;
        lv_obj_get_coords(last, &cardArea);
        lv_obj_get_coords(grid, &gridArea);
        lv_obj_get_coords(releaseAll, &releaseAfter);
        lv_obj_get_coords(back, &backAfter);
        desktopCheckGpio(lv_obj_get_scroll_y(grid) > 0 && cardArea.y1 >= gridArea.y1 && cardArea.y2 <= gridArea.y2 &&
                         releaseAfter.y1 == releaseBefore.y1 && backAfter.y1 == backBefore.y1,
                         "scroll hides controls or moves fixed actions");
        lv_obj_send_event(desktopFindButton(last, "Drive Low"), LV_EVENT_CLICKED, nullptr);
        auto* toggle = desktopFindSwitch(last);
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
        lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
        desktopCheckGpio(GetHAL()->gpioGetOutputState(51).high, "last row controls inaccessible");
        desktopSaveUiCapture("GPIO-scrolled");
        lv_obj_send_event(releaseAll, LV_EVENT_CLICKED, nullptr);
        desktopCheckGpio(!GetHAL()->gpioGetOutputState(51).claimed && GetHAL()->gpioGetOutputState(50).high &&
                         lv_obj_has_state(releaseAll, LV_STATE_DISABLED), "Release All changed unowned output");
        lv_obj_scroll_to_view(card, LV_ANIM_OFF);
        lv_obj_send_event(desktopFindButton(card, "Drive Low"), LV_EVENT_CLICKED, nullptr);
        toggle = desktopFindSwitch(card);
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
        lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
    }
}

static lv_obj_t* desktopFindLabelContaining(lv_obj_t* object, const char* text)
{
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(object, &lv_label_class) &&
        std::string(lv_label_get_text(object)).find(text) != std::string::npos) return object;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        if (auto* label = desktopFindLabelContaining(lv_obj_get_child(object, i), text)) return label;
    return nullptr;
}

static void desktopAssertScrollableText(lv_obj_t* label, const char* action, const char* capture)
{
    auto* root = desktopTopFullscreenRoot();
    auto* viewport = lv_obj_get_parent(label);
    auto* button = desktopFindButton(root, action);
    auto* back = desktopFindButton(root, "Back");
    lv_obj_update_layout(root);
    if (!lv_obj_has_flag(viewport, LV_OBJ_FLAG_SCROLLABLE)) std::abort();
    lv_obj_scroll_to_y(viewport, 0, LV_ANIM_OFF);
    lv_obj_update_layout(root);
    const auto bottom = lv_obj_get_scroll_bottom(viewport);
    if (bottom <= 0) std::abort();
    lv_area_t before, after;
    lv_obj_get_coords(button, &before);
    lv_obj_scroll_to_y(viewport, bottom, LV_ANIM_OFF);
    lv_obj_update_layout(root);
    lv_obj_get_coords(button, &after);
    if (before.y1 != after.y1 || lv_obj_get_scroll_bottom(viewport) > 1) std::abort();
    desktopAssertHitTarget(button);
    desktopAssertHitTarget(back);
    desktopSaveUiCapture(capture);
}

static void desktopAssertPopulatedTools(app_registry::AppKey key, int appId)
{
    if (key != app_registry::AppKey::SdCard && key != app_registry::AppKey::ComMonitor) return;
    const bool sd = key == app_registry::AppKey::SdCard;
    if (sd && !desktopEnvEnabled("M5TAB5_DESKTOP_SD_ROOT")) return;
    {
        LvglLockGuard lock;
        if (sd) {
            auto* scan = desktopFindButton(desktopTopFullscreenRoot(), "Scan SD Card");
            lv_obj_send_event(scan, LV_EVENT_CLICKED, nullptr);
            if (!lv_obj_has_state(scan, LV_STATE_DISABLED)) std::abort();
        } else {
            std::string log;
            for (int i = 0; i < 80; ++i) log += "serial row " + std::to_string(i) + " populated data\n";
            log += "SERIAL-END-MARKER\n";
            if (!GetHAL()->uartMonitorSend(log, false)) std::abort();
        }
    }
    const char* marker = sd ? "Showing first 256 entries" : "SERIAL-END-MARKER";
    bool found = false;
    const auto started = GetHAL()->millis();
    while (GetHAL()->millis() - started < 4000) {
        desktopPumpLifecycle();
        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            auto* label = desktopFindLabelContaining(root, marker);
            if (label) {
                if (!sd && std::strlen(lv_label_get_text(label)) > 2000) std::abort();
                desktopAssertScrollableText(label, sd ? "Scan SD Card" : "Send Test Message",
                                            sd ? "SD-populated-bottom" : "COM-populated-bottom");
                found = true;
                break;
            }
        }
        SDL_Delay(5);
    }
    if (!found) { mclog::tagError(_tag, "populated tool did not reach {}", marker); std::abort(); }
    if (!sd) {
        { LvglLockGuard lock; lv_obj_send_event(desktopFindButton(desktopTopFullscreenRoot(), "Back"), LV_EVENT_CLICKED, nullptr); }
        desktopPumpLifecycle();
        if (!app_registry::registry().open(key)) std::abort();
        desktopPumpLifecycle();
        LvglLockGuard lock;
        if (GetMooncake().getAppCurrentState(appId) != AppAbility::StateRunning ||
            !desktopHasLabelContaining(desktopTopFullscreenRoot(), marker)) std::abort();
        desktopSaveUiCapture("COM-reopened");
    }
}

static void desktopAssertAllRegisteredAppsUi()
{
    if (!_desktopAssertAllAppsUi || _desktopAllAppsUiAsserted) {
        return;
    }
    if (!desktopLauncherUiLooksReady()) {
        return;
    }

    auto& registry = app_registry::registry();
    {
        LvglLockGuard lock;
        desktopSaveUiCapture("Home");
    }
    for (const auto& registeredApp : registry.all()) {
        if (!registeredApp.visible || registeredApp.appId < 0) {
            continue;
        }

        const auto beforeOpen = desktopCaptureUiSnapshot();
        size_t timersBeforeOpen;
        {
            LvglLockGuard lock;
            timersBeforeOpen = desktopTimerCount();
        }
        if (registeredApp.key == app_registry::AppKey::Gpio) {
            desktopSeedIndependentGpio();
        }
        if (!registry.open(registeredApp.key)) {
            mclog::tagError(_tag, "desktop app UI smoke failed to open {}", registeredApp.label);
            std::abort();
        }
        desktopPumpLifecycle();

        const auto openSnapshot = desktopCaptureUiSnapshot();
        if (GetMooncake().getAppCurrentState(registeredApp.appId) != AppAbility::StateRunning ||
            openSnapshot.fullScreenChildCount < 1 || !desktopTopFullscreenRootHasContent()) {
            mclog::tagError(_tag, "desktop app UI smoke assertion failed for {} (fullscreen roots: {}, objects: {})",
                            registeredApp.label, static_cast<unsigned>(openSnapshot.fullScreenChildCount),
                            static_cast<unsigned>(openSnapshot.objectCount));
            std::abort();
        }
        if (!desktopValidateNonScrollableBounds(registeredApp.label.c_str())) {
            mclog::tagError(_tag, "desktop app UI smoke found off-bounds controls in {}", registeredApp.label);
            std::abort();
        }

        {
            LvglLockGuard lock;
            desktopSaveUiCapture(registeredApp.label);
        }
        if (registeredApp.key == app_registry::AppKey::Network) {
            desktopAssertNetworkSelection();
            if (!desktopValidateNonScrollableBounds("Network workflow")) {
                std::abort();
            }
        }
        if (registeredApp.key == app_registry::AppKey::Camera) {
            desktopAssertCameraWorkflow();
        }
        if (registeredApp.key == app_registry::AppKey::Rtc) {
            desktopAssertRtcWorkflow();
        }
        if (registeredApp.key == app_registry::AppKey::Updates) {
            desktopAssertUpdatesWorkflow(registeredApp.appId);
        }
        if (registeredApp.key == app_registry::AppKey::Pps) {
            desktopAssertPpsWorkflow();
        }
        if (registeredApp.key == app_registry::AppKey::Gpio) {
            desktopAssertGpioWorkflow(registeredApp.appId);
        }
        if (registeredApp.key == app_registry::AppKey::SettingsPower) {
            desktopAssertPowerWorkflow(registeredApp.appId);
        }
        desktopAssertPopulatedTools(registeredApp.key, registeredApp.appId);
        desktopAssertTouchTextEditing(registeredApp.label.c_str());

        {
            LvglLockGuard lock;
            auto* root = desktopTopFullscreenRoot();
            auto* header = lv_obj_get_child(root, 0);
            auto* back = lv_obj_get_child(header, 0);
            lv_area_t headerArea;
            lv_obj_get_coords(header, &headerArea);
            if (headerArea.y2 >= app_ui::safeContentRect().y ||
                lv_obj_has_flag(header, LV_OBJ_FLAG_HIDDEN) || !lv_obj_check_type(back, &lv_button_class)) {
                mclog::tagError(_tag, "desktop Back control is hidden in {}", registeredApp.label);
                std::abort();
            }
            lv_obj_send_event(back, LV_EVENT_CLICKED, nullptr);
        }
        desktopPumpLifecycle();

        if (GetMooncake().getAppCurrentState(registeredApp.appId) != AppAbility::StateSleeping) {
            mclog::tagError(_tag, "desktop app UI smoke failed to close {}", registeredApp.label);
            std::abort();
        }
        if (!desktopLauncherUiLooksReady()) {
            mclog::tagError(_tag, "desktop app UI smoke did not return to launcher after {}", registeredApp.label);
            std::abort();
        }
        const auto afterClose = desktopCaptureUiSnapshot();
        if (registeredApp.key == app_registry::AppKey::Gpio) {
            desktopCheckGpio(!GetHAL()->gpioGetOutputState(49).claimed && GetHAL()->gpioGetOutputState(50).high,
                             "Back cleanup released unowned output or left app output driven");
            desktopCheckGpio(GetHAL()->gpioReset(50), "independent output cleanup failed");
        }
        if (registeredApp.key == app_registry::AppKey::Pps && !desktopEnvEnabled("M5TAB5_DESKTOP_PPS_ABSENT")) {
            const auto started = GetHAL()->millis();
            uint8_t enabled = 1;
            while (GetHAL()->millis() - started < 2000) {
                if (GetHAL()->i2cReadRegister(hal::ModuleBus::MBus, 0x35, 0x04, &enabled, 1) && enabled == 0) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (enabled != 0) {
                mclog::tagError(_tag, "desktop PPS output remained on after Back");
                std::abort();
            }
            if (!registry.open(registeredApp.key)) {
                std::abort();
            }
            desktopPumpLifecycle();
            desktopWaitForPpsLabel("Output Off confirmed");
            {
                LvglLockGuard lock;
                auto* root = desktopTopFullscreenRoot();
                if (!desktopHasLabel(root, "Set V: 12.345 V")) {
                    mclog::tagError(_tag, "desktop PPS reopening must reflect verified hardware setpoints");
                    std::abort();
                }
            }
            desktopSetPpsOutput(true);
            desktopWaitForPpsLabel("Output On");
            setenv("M5TAB5_DESKTOP_PPS_OFF_FAIL", "1", 1);
            {
                LvglLockGuard lock;
                auto* root = desktopTopFullscreenRoot();
                lv_obj_send_event(desktopFindButton(root, "Back"), LV_EVENT_CLICKED, nullptr);
            }
            desktopPumpLifecycle();
            const auto warningStart = GetHAL()->millis();
            bool warningVisible = false;
            while (GetHAL()->millis() - warningStart < 2000) {
                desktopPumpLifecycle();
                {
                    LvglLockGuard lock;
                    warningVisible = desktopHasLabel(desktopTopFullscreenRoot(), "PPS OFF UNCONFIRMED - open PPS");
                }
                if (warningVisible) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!warningVisible) {
                mclog::tagError(_tag, "PPS off failure was lost after returning Home");
                std::abort();
            }
            {
                LvglLockGuard lock;
                desktopSaveUiCapture("Home-PPS-off-unconfirmed");
            }
            unsetenv("M5TAB5_DESKTOP_PPS_OFF_FAIL");
            if (!registry.open(registeredApp.key)) std::abort();
            desktopPumpLifecycle();
            desktopWaitForPpsLabel("Output Off confirmed");
            {
                LvglLockGuard lock;
                lv_obj_send_event(desktopFindButton(desktopTopFullscreenRoot(), "Back"), LV_EVENT_CLICKED, nullptr);
            }
            desktopPumpLifecycle();
            const auto reopenedClose = desktopCaptureUiSnapshot();
            if (reopenedClose.objectCount != beforeOpen.objectCount ||
                reopenedClose.fullScreenChildCount != beforeOpen.fullScreenChildCount) {
                std::abort();
            }
        }
        if (afterClose.objectCount != beforeOpen.objectCount ||
            afterClose.fullScreenChildCount != beforeOpen.fullScreenChildCount) {
            mclog::tagError(_tag, "desktop app UI objects leaked after closing {}", registeredApp.label);
            std::abort();
        }
        if (registeredApp.key == app_registry::AppKey::Camera) {
            {
                LvglLockGuard lock;
                if (GetHAL()->cameraGetStatus().state != hal::HalBase::CameraState::Stopped ||
                    desktopTimerCount() != timersBeforeOpen) {
                    mclog::tagError(_tag, "desktop camera leaked capture resources on close");
                    std::abort();
                }
            }
            // Also close before the first frame arrives, without pumping LVGL timers.
            if (!registry.open(registeredApp.key)) {
                std::abort();
            }
            desktopPumpLifecycle();
            desktopClickCameraAction("Start");
            {
                LvglLockGuard lock;
                if (GetHAL()->cameraGetStatus().state != hal::HalBase::CameraState::Starting) {
                    std::abort();
                }
                auto* back = desktopFindButton(desktopTopFullscreenRoot(), "Back");
                lv_obj_send_event(back, LV_EVENT_CLICKED, nullptr);
            }
            desktopPumpLifecycle();
            const auto closedBeforeFrame = desktopCaptureUiSnapshot();
            {
                LvglLockGuard lock;
                if (GetHAL()->cameraGetStatus().state != hal::HalBase::CameraState::Stopped ||
                    desktopTimerCount() != timersBeforeOpen || closedBeforeFrame.objectCount != beforeOpen.objectCount ||
                    closedBeforeFrame.fullScreenChildCount != beforeOpen.fullScreenChildCount) {
                    mclog::tagError(_tag, "desktop camera cleanup failed before first frame");
                    std::abort();
                }
            }
        }
    }

    _desktopAllAppsUiAsserted = true;
    mclog::tagInfo(_tag, "desktop registered app UI smoke assertion passed");
}
} // namespace

void desktop_smoke::configure(void (*pumpLifecycle)())
{
    _pumpLifecycle = pumpLifecycle;
    _desktopUpdateCount = 0;
    _desktopMaxUpdates = desktopMaxUpdatesFromEnv();
    _desktopAssertLauncherUi = desktopEnvEnabled("M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI");
    _desktopLauncherUiAsserted = false;
    _desktopAssertAllAppsUi = desktopEnvEnabled("M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI");
    _desktopAllAppsUiAsserted = false;
}

void desktop_smoke::afterUpdate()
{
    ++_desktopUpdateCount;
    if (_desktopMaxUpdates == 0 || _desktopUpdateCount + 1 >= _desktopMaxUpdates) {
        desktopAssertLauncherUiReady();
        desktopAssertAllRegisteredAppsUi();
    }
}

bool desktop_smoke::isDone()
{
    return _desktopMaxUpdates > 0 && _desktopUpdateCount >= _desktopMaxUpdates;
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_desktop.h"
#include "../hal_config.h"
#include "hal/hal.h"
#include <apps/runtime/performance_telemetry.h>
#include <mooncake_log.h>
#include <lvgl.h>
#include <mutex>
#include <thread>
#include <assets/assets.h>
// https://github.com/lvgl/lv_port_pc_vscode/blob/master/main/src/main.c

static const std::string _tag = "lvgl";
// Match the recursive display lock used by the Tab5 LVGL port.
static std::recursive_mutex _lvgl_mutex;

void HalDesktop::lvgl_init()
{
    mclog::tagInfo(_tag, "lvgl init");

    lv_init();

    lv_group_set_default(lv_group_create());

    auto display = lv_sdl_window_create(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_display_set_default(display);

    lvTouchpad = lv_sdl_mouse_create();
    lv_indev_set_group(lvTouchpad, lv_group_get_default());
    lv_indev_set_display(lvTouchpad, display);

    // // LV_IMAGE_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
    // lv_obj_t* cursor_obj;
    // cursor_obj = lv_image_create(lv_screen_active()); /*Create an image object for the cursor */
    // lv_image_set_src(cursor_obj, &mouse_cursor);      /*Set the image source*/
    // lv_indev_set_cursor(lvTouchpad, cursor_obj);      /*Connect the image  object to the driver*/

    auto mouse_wheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(mouse_wheel, display);
    lv_indev_set_group(mouse_wheel, lv_group_get_default());

    auto keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, display);
    lv_indev_set_group(keyboard, lv_group_get_default());


}

namespace {
thread_local unsigned displayLockDepth = 0;
thread_local uint64_t displayLockStartUs = 0;
thread_local uint64_t displayLockWaitUs = 0;
}
void HalDesktop::lvglLock()
{
    const auto started = app_runtime::monotonicMicros();
    _lvgl_mutex.lock();
    if (displayLockDepth++ == 0) {
        displayLockStartUs = app_runtime::monotonicMicros();
        displayLockWaitUs = displayLockStartUs - started;
    }
}

void HalDesktop::lvglUnlock()
{
    const bool outer = --displayLockDepth == 0;
    const auto held = app_runtime::monotonicMicros() - displayLockStartUs;
    _lvgl_mutex.unlock();
    if (outer) app_runtime::globalPerformanceTelemetry().recordLock(displayLockWaitUs, held);
}

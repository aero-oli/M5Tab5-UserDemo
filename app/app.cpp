/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app.h"
#include "hal/hal.h"
#include "apps/app_installer.h"
#include "apps/runtime/async_job_runner.h"
#include "apps/runtime/performance_telemetry.h"
#include "apps/app_pps/pps_controller.h"
#include "apps/app_settings_power/power_settings_controller.h"
#if defined(PLATFORM_BUILD_DESKTOP)
#include <test_support/ui_smoke.h>
#endif
#include <mooncake.h>
#include <mooncake_log.h>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <lvgl.h>
#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

using namespace mooncake;

static const std::string _tag = "app";
static lv_timer_t* _lifecycleTimer = nullptr;
static uint64_t _renderStartedUs = 0;

static void updateLifecycle(lv_timer_t*)
{
    const uint32_t started = GetHAL()->millis();
    GetMooncake().update();
    auto& perf = app_runtime::globalPerformanceTelemetry();
    perf.recordAppUpdate(started, GetHAL()->millis());
    perf.recordBackgroundJob(app_runtime::AsyncJobRunner::activeJobs() != 0,
                             app_runtime::globalAsyncJobRunner().currentJob());
#if defined(ESP_PLATFORM)
    static uint32_t lastResourcesMs = 0;
    if (started - lastResourcesMs >= 1000) {
        perf.recordResources(heap_caps_get_free_size(MALLOC_CAP_8BIT),
                             heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
                             uxTaskGetStackHighWaterMark(nullptr), uxTaskGetNumberOfTasks());
        lastResourcesMs = started;
    }
#endif
}

static void displayEvent(lv_event_t* event)
{
    const auto now = app_runtime::monotonicMicros();
    auto& perf = app_runtime::globalPerformanceTelemetry();
    switch (lv_event_get_code(event)) {
        case LV_EVENT_RENDER_START: _renderStartedUs = now; break;
        case LV_EVENT_RENDER_READY: perf.recordRender(_renderStartedUs, now); break;
        case LV_EVENT_FLUSH_FINISH: perf.recordFlush(now); break;
        default: break;
    }
}

void app::Init(InitCallback_t callback)
{
    mclog::tagInfo(_tag, "init");
#if defined(PLATFORM_BUILD_DESKTOP)
    desktop_smoke::configure([] {
        LvglLockGuard lock;
        lv_timer_ready(_lifecycleTimer);
        lv_timer_handler();
    });
#endif

    mclog::tagInfo(_tag, "hal injection");
    if (callback.onHalInjection) {
        callback.onHalInjection();
    }

    app_runtime::AsyncJobRunner::resumeAll();
    GetHAL()->resumeNetwork();
    GetHAL()->wifiRestoreSavedConnection();
    // Installation and timer registration are atomic with respect to the display task.
    // Thereafter all normal lifecycle transitions execute from this LVGL timer.
    LvglLockGuard lock;
    GetMooncake();
    on_install_apps();
    _lifecycleTimer = lv_timer_create(updateLifecycle, 16, nullptr);
    lv_timer_ready(_lifecycleTimer);
    lv_display_add_event_cb(lv_display_get_default(), displayEvent, LV_EVENT_ALL, nullptr);
    for (auto* input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input)) {
        lv_indev_add_event_cb(input, [](lv_event_t*) {
            app_runtime::globalPerformanceTelemetry().recordInput(app_runtime::monotonicMicros());
        }, LV_EVENT_PRESSED, nullptr);
    }
}

void app::Update()
{
#if defined(PLATFORM_BUILD_DESKTOP)
    // SDL and lifecycle callbacks share the main thread on every desktop platform.
    uint32_t next;
    {
        LvglLockGuard lock;
        const auto started = GetHAL()->millis();
        next = lv_timer_handler();
        app_runtime::globalPerformanceTelemetry().recordLvglHandler(started, GetHAL()->millis());
    }
    GetHAL()->delay(std::min<uint32_t>(next, 16));
#endif

#if defined(PLATFORM_BUILD_DESKTOP)
    desktop_smoke::afterUpdate();
#endif
}

bool app::IsDone()
{
#if defined(PLATFORM_BUILD_DESKTOP)
    return desktop_smoke::isDone();
#else
    return false;
#endif
}

void app::Destroy()
{
    {
        LvglLockGuard lock;
        if (_lifecycleTimer) lv_timer_delete(_lifecycleTimer);
        _lifecycleTimer = nullptr;
        DestroyMooncake();
    }
    GetHAL()->audioCancel();
    if (!GetHAL()->shutdownCamera(5000)) {
        mclog::tagError(_tag, "Camera shutdown timed out: refusing to destroy HAL while capture is active");
        std::abort();
    }
    app_pps::PpsController::shutdown();
    app_settings_power::PowerSettingsController::shutdown();
    if (!GetHAL()->stopNetworkAndWait(15000)) {
        mclog::tagError(_tag, "Network shutdown timed out: refusing to destroy active update resources");
        std::abort();
    }
    if (!GetHAL()->stopUartAndWait(2000)) {
        mclog::tagError(_tag, "UART shutdown timed out: refusing to destroy active serial resources");
        std::abort();
    }
    if (!app_runtime::AsyncJobRunner::shutdownAll(30000)) {
        mclog::tagError(_tag, "Shutdown timed out: refusing to destroy HAL while jobs still use it");
        std::abort();
    }
    hal::Destroy();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <app.h>
#include <hal/hal.h>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <bsp/m5stack_tab5.h>
#include <apps/runtime/boot_health.h>
#include <apps/runtime/performance_telemetry.h>
#include <apps/app_registry.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr const char* bootTag = "boot-health";
constexpr uint64_t pendingBootTimeoutUs = 15000000; // Includes HAL/app initialization and diagnostics.
std::atomic<bool> imageConfirmed{false};
esp_timer_handle_t pendingBootWatchdog = nullptr;

bool pendingImage()
{
    const auto* running = esp_ota_get_running_partition();
    if (!running) { ESP_LOGE(bootTag, "Cannot identify running image"); std::abort(); }
    if (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) return false;
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const auto result = esp_ota_get_state_partition(running, &state);
    if (result != ESP_OK) {
        ESP_LOGE(bootTag, "Cannot read OTA image state: %s; image remains unconfirmed", esp_err_to_name(result));
        std::abort();
    }
    return state == ESP_OTA_IMG_PENDING_VERIFY;
}
[[noreturn]] void rejectImage(const char* reason)
{
    ESP_LOGE(bootTag, "%s; requesting rollback", reason);
    const auto result = esp_ota_mark_app_invalid_rollback_and_reboot();
    // Successful rollback does not return. Never convert a rollback/write failure
    // into a valid image; a later reset still rejects a pending image.
    ESP_LOGE(bootTag, "Rollback did not reboot: %s; leaving image unconfirmed", esp_err_to_name(result));
    std::abort();
}
void armPendingBootWatchdog()
{
    // Installed IDF 5.4: esp_timer_init_os is a SECONDARY system init function;
    // startup.c completes do_secondary_init() before FreeRTOS calls app_main().
    // This independent timer remains live while app::Init or a display lock hangs.
    esp_timer_create_args_t args{};
    args.name = "pending_boot";
    args.callback = [](void*) {
        // Never take a display/log mutex or inspect flash from the timer task.
        // A reset with PENDING_VERIFY makes the rollback-enabled bootloader reject it.
        if (!imageConfirmed.load(std::memory_order_acquire)) esp_restart();
    };
    const auto created = esp_timer_create(&args, &pendingBootWatchdog);
    if (created != ESP_OK) rejectImage("Could not create independent pending-image watchdog");
    if (esp_timer_start_once(pendingBootWatchdog, pendingBootTimeoutUs) != ESP_OK)
        rejectImage("Could not arm independent pending-image watchdog");
}
void disarmConfirmedBootWatchdog()
{
    // Publish only after successful mark-valid AND state readback. If expiry races
    // the flash commit it can cause an extra reboot; IDF's redundant otadata decides
    // whether the healthy image or its predecessor boots. No flash atomicity claim.
    imageConfirmed.store(true, std::memory_order_release);
    const auto stopped = esp_timer_stop(pendingBootWatchdog);
    if (stopped != ESP_OK && stopped != ESP_ERR_INVALID_STATE)
        ESP_LOGW(bootTag, "Pending watchdog stop returned %s", esp_err_to_name(stopped));
    const auto deleted = esp_timer_delete(pendingBootWatchdog);
    if (deleted != ESP_OK) ESP_LOGW(bootTag, "Pending watchdog delete returned %s", esp_err_to_name(deleted));
    pendingBootWatchdog = nullptr;
}
void verifyPendingImage()
{
    ESP_LOGI(bootTag, "Pending image: observing core UI/input startup for at least 3 seconds");
    lv_timer_t* probe = nullptr;
    {
        LvglLockGuard lock;
        probe = lv_timer_create([](lv_timer_t*) {
            auto* display = static_cast<HalEsp32*>(GetHAL())->lvDisp;
            if (display) lv_obj_invalidate(lv_display_get_screen_active(display));
        }, 250, nullptr);
    }
    const auto initial = app_runtime::globalPerformanceTelemetry().snapshot();
    app_runtime::BootHealthGate gate(GetHAL()->millis(), initial.totalAppUpdates, initial.renderedFrames);
    for (;;) {
        const auto sample = app_runtime::globalPerformanceTelemetry().snapshot();
        const bool coreReady = probe && static_cast<HalEsp32*>(GetHAL())->lvDisp && bsp_display_get_input_dev() &&
                               !app_registry::registry().all().empty();
        const auto decision = gate.observe(GetHAL()->millis(), sample.totalAppUpdates, sample.renderedFrames, coreReady);
        if (decision == app_runtime::BootHealthGate::Decision::Reject)
            rejectImage("Core startup diagnostics timed out"); // No display lock on this failure path.
        if (decision == app_runtime::BootHealthGate::Decision::Accept) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    {
        LvglLockGuard lock;
        lv_timer_delete(probe);
    }
    const auto result = esp_ota_mark_app_valid_cancel_rollback();
    if (result != ESP_OK) rejectImage("Could not persist startup confirmation");
    esp_ota_img_states_t actual = ESP_OTA_IMG_UNDEFINED;
    const auto* running = esp_ota_get_running_partition();
    if (!running || esp_ota_get_state_partition(running, &actual) != ESP_OK || actual != ESP_OTA_IMG_VALID)
        rejectImage("Startup confirmation readback failed");
    disarmConfirmedBootWatchdog();
    ESP_LOGI(bootTag, "Core UI/input scheduling and rendering passed; image marked valid (peripherals unverified)");
}
} // namespace

extern "C" void app_main(void)
{
    // IDF 5.4 app_update/bootloader rollback flow: only PENDING_VERIFY images
    // are confirmed, after observed progress rather than merely entering boot.
    // Requires the rollback-enabled bootloader to have been flashed as well.
    const bool needsVerification = pendingImage();
    if (needsVerification) armPendingBootWatchdog();
    // 应用层初始化回调
    app::InitCallback_t callback;

    callback.onHalInjection = []() {
        // 注入桌面平台的硬件抽象
        hal::Inject(std::make_unique<HalEsp32>());
    };

    // 应用层启动
    app::Init(callback);
    if (needsVerification) verifyPendingImage();
    while (!app::IsDone()) {
        app::Update();
        vTaskDelay(1);
    }
    app::Destroy();
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_imu.h"
#include "imu_controller.h"
#include "imu_layout.h"

#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_hardware_imu;

namespace {

class AppImu : public mooncake::AppAbility {
public:
    AppImu()
        : _controller([] { GetHAL()->updateImuData(); },
                      [] { return GetHAL()->getImuData(); })
    {
        setAppInfo().name = "IMU";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("IMU", [&]() { close(); });
        _shell->init();
        build();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 250) {
            return;
        }
        LvglLockGuard lock;
        refresh();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _shell.reset();
    }

private:
    ImuController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _accelX = nullptr;
    lv_obj_t* _accelY = nullptr;
    lv_obj_t* _accelZ = nullptr;
    lv_obj_t* _gyroX = nullptr;
    lv_obj_t* _gyroY = nullptr;
    lv_obj_t* _gyroZ = nullptr;
    lv_obj_t* _detail = nullptr;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    void build()
    {
        LvglLockGuard lock;
        const auto layout = makeImuLayout();
        auto* description = app_ui::makeLabel(content(), "Live accelerometer and gyro readout.", layout.description.x,
                                              layout.description.y, &lv_font_montserrat_20,
                                              app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(description, layout.description.w);
        lv_label_set_long_mode(description, LV_LABEL_LONG_DOT);
        _accelX = makeMetric(layout, 0, "Accel X");
        _accelY = makeMetric(layout, 1, "Accel Y");
        _accelZ = makeMetric(layout, 2, "Accel Z");
        _gyroX = makeMetric(layout, 3, "Gyro X");
        _gyroY = makeMetric(layout, 4, "Gyro Y");
        _gyroZ = makeMetric(layout, 5, "Gyro Z");
        _detail = app_ui::makeLabel(content(), "Sampling: 4 Hz while this app is open", layout.detailText.x,
                                    layout.detailText.y, &lv_font_montserrat_18,
                                    app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.detailText.w);
        lv_label_set_long_mode(_detail, LV_LABEL_LONG_DOT);
        refresh();
    }

    lv_obj_t* makeMetric(const ImuLayout& layout, int index, const char* title)
    {
        const auto rect = imuMetricRect(layout, index);
        return app_ui::makeMetricCard(content(), title, "--", rect.x, rect.y, rect.w, rect.h);
    }

    void refresh()
    {
        const auto sample = _controller.sample();
        lv_label_set_text(_detail, ImuController::statusText(sample, GetHAL()->millis()).c_str());
        setMetric(_accelX, "Accel X", sample.hasSample ? ImuController::formatAccel(sample.accelX).c_str() : "--");
        setMetric(_accelY, "Accel Y", sample.hasSample ? ImuController::formatAccel(sample.accelY).c_str() : "--");
        setMetric(_accelZ, "Accel Z", sample.hasSample ? ImuController::formatAccel(sample.accelZ).c_str() : "--");
        setMetric(_gyroX, "Gyro X", sample.hasSample ? ImuController::formatGyro(sample.gyroX).c_str() : "--");
        setMetric(_gyroY, "Gyro Y", sample.hasSample ? ImuController::formatGyro(sample.gyroY).c_str() : "--");
        setMetric(_gyroZ, "Gyro Z", sample.hasSample ? ImuController::formatGyro(sample.gyroZ).c_str() : "--");
    }

    void setMetric(lv_obj_t* card, const char* title, const char* value)
    {
        if (card == nullptr) {
            return;
        }
        auto* titleLabel = lv_obj_get_child(card, 0);
        if (titleLabel != nullptr) {
            lv_label_set_text(titleLabel, title);
        }
        auto* valueLabel = lv_obj_get_child(card, 1);
        if (valueLabel != nullptr) {
            lv_label_set_text(valueLabel, value);
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_hardware_imu::makeImuApp()
{
    return std::make_unique<AppImu>();
}

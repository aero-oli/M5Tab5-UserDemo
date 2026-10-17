/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <mooncake_log.h>
#include <vector>
#include <driver/gpio.h>
#include <memory>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "accel_gyro_bmi270.h"

static const std::string _tag = "imu";

void HalEsp32::clearImuIrq()
{
    mclog::tagInfo(_tag, "clear imu irq");
    if (accel_gyro_bmi270_init(bsp_i2c_get_handle()) != ESP_OK) {
        mclog::tagError(_tag, "IMU initialization failed");
        return;
    }
    if (accel_gyro_bmi270_check_irq()) {
        accel_gyro_bmi270_clear_irq_int();
        mclog::tagInfo(_tag, "imu irq detected! clear it!");
    }
}

void HalEsp32::imu_init()
{
    mclog::tagInfo(_tag, "imu init");

    if (accel_gyro_bmi270_init(bsp_i2c_get_handle()) != ESP_OK) {
        mclog::tagError(_tag, "IMU initialization failed");
        return;
    }
    if (accel_gyro_bmi270_check_irq()) {
        accel_gyro_bmi270_clear_irq_int();
        mclog::tagInfo(_tag, "imu irq detected! clear it!");
    }
    if (!accel_gyro_bmi270_enable_sensor()) mclog::tagError(_tag, "IMU normal sampling configuration failed");
}

void HalEsp32::updateImuData()
{
    std::lock_guard<std::mutex> lock(_imuIoMutex);
    IMUData_t data;
    struct bmi2_sens_data bmi_sensor_data = {};
    if (!accel_gyro_bmi270_get_data(&bmi_sensor_data)) {
        publishImuError("IMU read failed");
        return;
    }

    /* 根据设置量程转换 */
    data.accelX = bmi_sensor_data.acc.y / 835.92 / 10.0f;  // m/s^2
    data.accelY = -bmi_sensor_data.acc.x / 835.92 / 10.0f;
    data.accelZ = -bmi_sensor_data.acc.z / 835.92 / 10.0f;
    data.gyroX  = bmi_sensor_data.gyr.y / 32.768 / 10.0f;  // °/s   gyro_raw*2*1000/2^16 --> 0.0305
    data.gyroY  = bmi_sensor_data.gyr.x / 32.768 / 10.0f;
    data.gyroZ  = -bmi_sensor_data.gyr.z / 32.768 / 10.0f;
    data.valid = data.hasSample = true;
    data.sampledAtMs            = millis();
    publishImuData(data);
}

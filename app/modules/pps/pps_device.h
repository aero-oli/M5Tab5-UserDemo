/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <modules/i2c_device.h>

namespace pps {

static constexpr uint8_t DEFAULT_I2C_ADDRESS = 0x35;
static constexpr uint16_t EXPECTED_MODULE_ID = 0x1041;

static constexpr uint8_t REG_MODULE_ID_L     = 0x00;
static constexpr uint8_t REG_MODULE_ENABLE   = 0x04;
static constexpr uint8_t REG_RUNNING_MODE    = 0x05;
static constexpr uint8_t REG_DATA_FLAG       = 0x07;
static constexpr uint8_t REG_VOUT_READBACK   = 0x08;
static constexpr uint8_t REG_IOUT_READBACK   = 0x0C;
static constexpr uint8_t REG_TEMP_READBACK   = 0x10;
static constexpr uint8_t REG_VIN_READBACK    = 0x14;
static constexpr uint8_t REG_VOUT_SET        = 0x18;
static constexpr uint8_t REG_IOUT_SET        = 0x1C;
static constexpr uint8_t REG_UID_W0          = 0x52;
static constexpr uint8_t REG_I2C_ADDRESS     = 0x5F;

static constexpr float MIN_OUTPUT_VOLTAGE = 0.5f;
static constexpr float MAX_OUTPUT_VOLTAGE = 30.0f;
static constexpr float MIN_OUTPUT_CURRENT = 0.0f;
static constexpr float MAX_OUTPUT_CURRENT = 5.0f;

enum class PpsMode : uint8_t {
    Disabled        = 0,
    ConstantVoltage = 1,
    ConstantCurrent = 2,
    Unknown         = 255,
};

struct PpsIdentity {
    uint16_t moduleId = 0;
    std::string uid;
    uint8_t i2cAddress = DEFAULT_I2C_ADDRESS;
};

struct PpsSetpoints {
    float voltage = 5.0f;
    float current = 1.0f;
};

struct PpsTelemetry {
    bool connected = false;
    bool outputEnabled = false;
    PpsMode mode = PpsMode::Disabled;
    uint8_t dataFlag = 0;
    float vin = 0.0f;
    float temperature = 0.0f;
    float voutReadback = 0.0f;
    float ioutReadback = 0.0f;
    float outputPower = 0.0f;
    PpsSetpoints setpoints;
    std::string lastError;
};

class PpsDevice {
public:
    explicit PpsDevice(modules::I2cDevice& device);

    bool probe();
    bool readIdentity(PpsIdentity& identity, const std::function<bool()>& interrupted = {});
    bool readTelemetry(PpsTelemetry& telemetry, const std::function<bool()>& interrupted = {});
    bool setOutputEnabled(bool enabled);
    bool readOutputEnabled(bool& enabled);
    bool writeSetpoints(float voltage, float current, const std::function<bool()>& interrupted = {});
    bool readSetpoints(PpsSetpoints& setpoints, const std::function<bool()>& interrupted = {});

    static float clampVoltage(float voltage);
    static float clampCurrent(float current);
    static const char* modeName(PpsMode mode);

private:
    modules::I2cDevice& _device;

    bool readByte(uint8_t reg, uint8_t& value);
    bool writeByte(uint8_t reg, uint8_t value);
    bool readFloat(uint8_t reg, float& value);
    bool writeFloat(uint8_t reg, float value);
    static PpsMode parseMode(uint8_t mode);
};

}  // namespace pps

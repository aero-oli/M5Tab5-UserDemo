/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "pps_device.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace pps;

PpsDevice::PpsDevice(modules::I2cDevice& device) : _device(device) {}

bool PpsDevice::probe()
{
    return _device.probe();
}

bool PpsDevice::readIdentity(PpsIdentity& identity, const std::function<bool()>& interrupted)
{
    uint8_t id_bytes[2] = {};
    uint8_t uid_bytes[12] = {};

    if ((interrupted && interrupted()) || !_device.probe()) {
        return false;
    }
    if ((interrupted && interrupted()) || !_device.readRegister(REG_MODULE_ID_L, id_bytes, sizeof(id_bytes))) {
        return false;
    }
    if ((interrupted && interrupted()) || !_device.readRegister(REG_UID_W0, uid_bytes, sizeof(uid_bytes))) {
        return false;
    }

    identity.moduleId = static_cast<uint16_t>(id_bytes[0]) | (static_cast<uint16_t>(id_bytes[1]) << 8);
    identity.i2cAddress = _device.address();

    uint32_t uid0 = static_cast<uint32_t>(uid_bytes[0]) | (static_cast<uint32_t>(uid_bytes[1]) << 8) |
                    (static_cast<uint32_t>(uid_bytes[2]) << 16) | (static_cast<uint32_t>(uid_bytes[3]) << 24);
    uint32_t uid1 = static_cast<uint32_t>(uid_bytes[4]) | (static_cast<uint32_t>(uid_bytes[5]) << 8) |
                    (static_cast<uint32_t>(uid_bytes[6]) << 16) | (static_cast<uint32_t>(uid_bytes[7]) << 24);
    uint32_t uid2 = static_cast<uint32_t>(uid_bytes[8]) | (static_cast<uint32_t>(uid_bytes[9]) << 8) |
                    (static_cast<uint32_t>(uid_bytes[10]) << 16) | (static_cast<uint32_t>(uid_bytes[11]) << 24);

    char uid[32] = {};
    snprintf(uid, sizeof(uid), "%08" PRIX32 "-%08" PRIX32 "-%08" PRIX32, uid0, uid1, uid2);
    identity.uid = uid;

    return true;
}

bool PpsDevice::readTelemetry(PpsTelemetry& telemetry, const std::function<bool()>& interrupted)
{
    telemetry = PpsTelemetry();

    if (!_device.probe()) {
        telemetry.lastError = "No PPS detected / check 9-36V input power";
        return false;
    }

    telemetry.connected = true;

    uint8_t enabled = 0;
    uint8_t mode = 0;
    auto proceed = [&] { return !interrupted || !interrupted(); };
    if (!proceed() || !readByte(REG_MODULE_ENABLE, enabled) || !proceed() || !readByte(REG_RUNNING_MODE, mode) ||
        !proceed() || !readByte(REG_DATA_FLAG, telemetry.dataFlag) || !proceed() ||
        !readFloat(REG_VOUT_READBACK, telemetry.voutReadback) || !proceed() ||
        !readFloat(REG_IOUT_READBACK, telemetry.ioutReadback) || !proceed() ||
        !readFloat(REG_TEMP_READBACK, telemetry.temperature) || !proceed() ||
        !readFloat(REG_VIN_READBACK, telemetry.vin) || !proceed() ||
        !readFloat(REG_VOUT_SET, telemetry.setpoints.voltage) || !proceed() ||
        !readFloat(REG_IOUT_SET, telemetry.setpoints.current)) {
        telemetry.connected = false;
        telemetry.lastError = "PPS read failed";
        return false;
    }

    if (enabled > 1 || telemetry.setpoints.voltage < MIN_OUTPUT_VOLTAGE ||
        telemetry.setpoints.voltage > MAX_OUTPUT_VOLTAGE || telemetry.setpoints.current < MIN_OUTPUT_CURRENT ||
        telemetry.setpoints.current > MAX_OUTPUT_CURRENT) {
        telemetry.connected = false;
        telemetry.lastError = "Invalid PPS register data";
        return false;
    }
    telemetry.outputEnabled = enabled != 0;
    telemetry.mode = parseMode(mode);
    telemetry.outputPower = telemetry.voutReadback * telemetry.ioutReadback;
    return true;
}

bool PpsDevice::setOutputEnabled(bool enabled)
{
    return writeByte(REG_MODULE_ENABLE, enabled ? 0x01 : 0x00);
}

bool PpsDevice::readOutputEnabled(bool& enabled)
{
    uint8_t value = 0;
    if (!readByte(REG_MODULE_ENABLE, value)) {
        return false;
    }
    if (value > 1) {
        return false;
    }
    enabled = value != 0;
    return true;
}

bool PpsDevice::writeSetpoints(float voltage, float current, const std::function<bool()>& interrupted)
{
    if (!std::isfinite(voltage) || !std::isfinite(current)) {
        return false;
    }
    return (!interrupted || !interrupted()) && writeFloat(REG_VOUT_SET, clampVoltage(voltage)) &&
           (!interrupted || !interrupted()) && writeFloat(REG_IOUT_SET, clampCurrent(current));
}

bool PpsDevice::readSetpoints(PpsSetpoints& setpoints, const std::function<bool()>& interrupted)
{
    return (!interrupted || !interrupted()) && readFloat(REG_VOUT_SET, setpoints.voltage) &&
           (!interrupted || !interrupted()) && readFloat(REG_IOUT_SET, setpoints.current);
}

float PpsDevice::clampVoltage(float voltage)
{
    return std::clamp(voltage, MIN_OUTPUT_VOLTAGE, MAX_OUTPUT_VOLTAGE);
}

float PpsDevice::clampCurrent(float current)
{
    return std::clamp(current, MIN_OUTPUT_CURRENT, MAX_OUTPUT_CURRENT);
}

const char* PpsDevice::modeName(PpsMode mode)
{
    switch (mode) {
        case PpsMode::Disabled:
            return "Disabled";
        case PpsMode::ConstantVoltage:
            return "CV";
        case PpsMode::ConstantCurrent:
            return "CC";
        default:
            return "Unknown";
    }
}

bool PpsDevice::readByte(uint8_t reg, uint8_t& value)
{
    return _device.readRegister(reg, &value, 1);
}

bool PpsDevice::writeByte(uint8_t reg, uint8_t value)
{
    return _device.writeRegister(reg, &value, 1);
}

bool PpsDevice::readFloat(uint8_t reg, float& value)
{
    uint8_t bytes[4] = {};
    if (!_device.readRegister(reg, bytes, sizeof(bytes))) {
        return false;
    }
    const uint32_t bits = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                          (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
    static_assert(sizeof(value) == sizeof(bits), "PPS requires 32-bit floats");
    memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value);
}

bool PpsDevice::writeFloat(uint8_t reg, float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(value));
    uint8_t bytes[4] = {static_cast<uint8_t>(bits), static_cast<uint8_t>(bits >> 8),
                        static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 24)};
    return _device.writeRegister(reg, bytes, sizeof(bytes));
}

PpsMode PpsDevice::parseMode(uint8_t mode)
{
    switch (mode) {
        case 0:
            return PpsMode::Disabled;
        case 1:
            return PpsMode::ConstantVoltage;
        case 2:
            return PpsMode::ConstantCurrent;
        default:
            return PpsMode::Unknown;
    }
}

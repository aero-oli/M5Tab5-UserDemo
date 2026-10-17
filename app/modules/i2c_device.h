/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <hal/hal.h>

namespace modules {

class I2cDevice {
public:
    virtual ~I2cDevice() = default;
    virtual bool probe() = 0;
    virtual uint8_t address() const = 0;
    virtual bool readRegister(uint8_t reg, uint8_t* buffer, size_t length) = 0;
    virtual bool writeRegister(uint8_t reg, const uint8_t* data, size_t length) = 0;
};

class HalI2cDevice : public I2cDevice {
public:
    HalI2cDevice(hal::ModuleBus bus, uint8_t address);

    bool probe() override;
    uint8_t address() const override;
    bool readRegister(uint8_t reg, uint8_t* buffer, size_t length) override;
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t length) override;

private:
    hal::ModuleBus _bus;
    uint8_t _address;
};

}  // namespace modules

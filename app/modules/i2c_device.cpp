/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "i2c_device.h"

using namespace modules;

HalI2cDevice::HalI2cDevice(hal::ModuleBus bus, uint8_t address) : _bus(bus), _address(address)
{
}

bool HalI2cDevice::probe()
{
    return GetHAL()->i2cProbe(_bus, _address);
}

uint8_t HalI2cDevice::address() const
{
    return _address;
}

bool HalI2cDevice::readRegister(uint8_t reg, uint8_t* buffer, size_t length)
{
    return GetHAL()->i2cReadRegister(_bus, _address, reg, buffer, length);
}

bool HalI2cDevice::writeRegister(uint8_t reg, const uint8_t* data, size_t length)
{
    return GetHAL()->i2cWriteRegister(_bus, _address, reg, data, length);
}

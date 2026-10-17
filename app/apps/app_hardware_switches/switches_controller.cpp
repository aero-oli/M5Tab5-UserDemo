/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "switches_controller.h"

#include <cstdio>

using namespace app_hardware_switches;

SwitchesController::SwitchesController(ReadSwitchesFn readSwitchesFn, SetSwitchFn setSwitchFn, ReadInterfaceFn readInterfaceFn)
    : _readSwitchesFn(std::move(readSwitchesFn)), _setSwitchFn(std::move(setSwitchFn)),
      _readInterfaceFn(std::move(readInterfaceFn))
{
}

SwitchSnapshot SwitchesController::readSwitches() const
{
    return _readSwitchesFn();
}

void SwitchesController::setSwitch(SwitchId id, bool enabled)
{
    _setSwitchFn(id, enabled);
}

InterfaceSnapshot SwitchesController::readInterface() const
{
    return _readInterfaceFn();
}

std::string SwitchesController::formatInterfaceStatus(const InterfaceSnapshot& snapshot)
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "USB-C: %s    USB-A: %s    Headphone: %s",
                  snapshot.usbC ? "connected" : "off", snapshot.usbA ? "connected" : "off",
                  snapshot.headphone ? "connected" : "off");
    return buffer;
}

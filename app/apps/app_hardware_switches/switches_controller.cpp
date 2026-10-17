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
    const bool available = snapshot.valid && snapshot.hasSample;
    const auto label = [available](bool known, bool connected) {
        return !available || !known ? "unknown" : connected ? "connected" : "off";
    };
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%sUSB-C: %s    USB-A: %s    Headphone: %s%s",
                  snapshot.simulated ? "Simulated: " : "",
                  label(snapshot.usbCKnown, snapshot.usbC), label(snapshot.usbAKnown, snapshot.usbA),
                  label(snapshot.headphoneKnown, snapshot.headphone),
                  !snapshot.valid && snapshot.hasSample ? " (stale)" : "");
    return buffer;
}

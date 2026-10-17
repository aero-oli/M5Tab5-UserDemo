/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>
#include <hal/hal.h>

namespace app_hardware_switches {

enum class SwitchId {
    Charge,
    ChargeQc,
    Ext5v,
    UsbA5v,
    ExtAntenna,
};

struct SwitchSnapshot {
    bool charge = false;
    bool chargeQc = false;
    bool ext5v = false;
    bool usbA5v = false;
    bool extAntenna = false;
};

using InterfaceSnapshot = hal::HalBase::InterfaceData;

class SwitchesController {
public:
    using ReadSwitchesFn = std::function<SwitchSnapshot()>;
    using SetSwitchFn = std::function<void(SwitchId id, bool enabled)>;
    using ReadInterfaceFn = std::function<InterfaceSnapshot()>;

    SwitchesController(ReadSwitchesFn readSwitchesFn, SetSwitchFn setSwitchFn, ReadInterfaceFn readInterfaceFn);

    SwitchSnapshot readSwitches() const;
    void setSwitch(SwitchId id, bool enabled);
    InterfaceSnapshot readInterface() const;

    static std::string formatInterfaceStatus(const InterfaceSnapshot& snapshot);

private:
    ReadSwitchesFn _readSwitchesFn;
    SetSwitchFn _setSwitchFn;
    ReadInterfaceFn _readInterfaceFn;
};

}  // namespace app_hardware_switches

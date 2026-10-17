/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <hal/gpio_diagnostics.h>

namespace app_hardware_gpio {

struct GpioPinState {
    hal::GpioPinInfo info;
    bool claimed    = false;
    bool levelKnown = false;
    bool high       = false;
};

class GpioController {
public:
    using InitFn  = std::function<hal::GpioClaimResult(uint8_t pin)>;
    using SetFn   = std::function<bool(uint8_t pin, bool level)>;
    using ResetFn = std::function<bool(uint8_t pin)>;

    GpioController(std::vector<hal::GpioPinInfo> pins, InitFn initFn, SetFn setFn, ResetFn resetFn);
    ~GpioController();
    GpioController(const GpioController&) = delete;
    GpioController& operator=(const GpioController&) = delete;

    bool claim(uint8_t pin);
    bool setLevel(uint8_t pin, bool level);
    bool release(uint8_t pin);
    bool resetAll();
    bool isKnownPin(uint8_t pin) const;

    const std::vector<GpioPinState>& pins() const;
    const GpioPinState* state(uint8_t pin) const;
    size_t claimedCount() const;
    const std::string& lastError() const;

private:
    InitFn _initFn;
    SetFn _setFn;
    ResetFn _resetFn;
    std::vector<GpioPinState> _pins;
    std::string _lastError;
    GpioPinState* find(uint8_t pin);
};

}  // namespace app_hardware_gpio

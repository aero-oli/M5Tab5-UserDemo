/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "gpio_controller.h"
#include <algorithm>

using namespace app_hardware_gpio;

GpioController::GpioController(std::vector<hal::GpioPinInfo> pins, InitFn initFn, SetFn setFn, ResetFn resetFn)
    : _initFn(std::move(initFn)), _setFn(std::move(setFn)), _resetFn(std::move(resetFn))
{
    for (auto& pin : pins) {
        if (!find(pin.pin)) {
            _pins.push_back({std::move(pin)});
        }
    }
}

GpioController::~GpioController()
{
    resetAll();
}

GpioPinState* GpioController::find(uint8_t pin)
{
    auto found = std::find_if(_pins.begin(), _pins.end(), [pin](const auto& state) { return state.info.pin == pin; });
    return found == _pins.end() ? nullptr : &*found;
}

const GpioPinState* GpioController::state(uint8_t pin) const
{
    const auto found =
        std::find_if(_pins.begin(), _pins.end(), [pin](const auto& state) { return state.info.pin == pin; });
    return found == _pins.end() ? nullptr : &*found;
}

bool GpioController::claim(uint8_t pin)
{
    auto* state = find(pin);
    if (!state || !state->info.outputAllowed || state->claimed || !_initFn) {
        _lastError = "GPIO unavailable or already claimed";
        return false;
    }
    const auto result = _initFn(pin);
    // Rejection changes nothing; partial configuration retains ownership for cleanup/retry.
    state->claimed    = result != hal::GpioClaimResult::Rejected;
    state->levelKnown = result == hal::GpioClaimResult::Claimed;
    state->high       = false;
    _lastError        = state->levelKnown ? ""
                        : state->claimed  ? "GPIO initialization failed; state unknown"
                                          : "GPIO reserved or busy";
    return state->levelKnown;
}

bool GpioController::setLevel(uint8_t pin, bool level)
{
    auto* state = find(pin);
    if (!state || !state->info.outputAllowed || !state->claimed || !state->levelKnown || !_setFn) {
        _lastError = "GPIO is not a ready claimed output";
        return false;
    }
    if (state->high == level) {
        return true;
    }
    if (!_setFn(pin, level)) {
        state->levelKnown = false;
        _lastError        = "GPIO write failed; state unknown";
        return false;
    }
    state->high = level;
    _lastError.clear();
    return true;
}

bool GpioController::release(uint8_t pin)
{
    auto* state = find(pin);
    if (!state || !state->info.outputAllowed || !state->claimed || !_resetFn) {
        return false;
    }
    if (!_resetFn(pin)) {
        state->levelKnown = false;
        _lastError        = "GPIO release failed; pin may still be driven";
        return false;
    }
    state->claimed    = false;
    state->levelKnown = false;
    state->high       = false;
    _lastError.clear();
    return true;
}

bool GpioController::resetAll()
{
    bool ok = true;
    for (auto& state : _pins) {
        if (state.claimed && !release(state.info.pin)) {
            ok = false;
        }
    }
    if (!ok) {
        _lastError = "GPIO release failed; a pin may still be driven";
    }
    return ok;
}

bool GpioController::isKnownPin(uint8_t pin) const
{
    return state(pin) != nullptr;
}
const std::vector<GpioPinState>& GpioController::pins() const
{
    return _pins;
}
const std::string& GpioController::lastError() const
{
    return _lastError;
}
size_t GpioController::claimedCount() const
{
    return std::count_if(_pins.begin(), _pins.end(), [](const auto& state) { return state.claimed; });
}

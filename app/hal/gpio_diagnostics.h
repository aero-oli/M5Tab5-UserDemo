#pragma once
#include <cstdint>
#include <string>

namespace hal {

enum class GpioClaimResult { Rejected, Claimed, Unknown };

struct GpioPinInfo {
    uint8_t pin;
    std::string role;
    bool outputAllowed;
    std::string reservation;
};

struct GpioOutputState {
    bool claimed = false;
    bool levelKnown = false;
    bool high = false;
};

}  // namespace hal

#pragma once
#include <hal/gpio_diagnostics.h>
#include <algorithm>
#include <vector>

namespace hal {

// Expansion inventory; internal peripherals are deliberately not available to GPIO diagnostics.
// Board map: https://docs.m5stack.com/en/core/Tab5#pinmap
inline std::vector<GpioPinInfo> tab5ExpansionPins()
{
    return {
        {49, "GPIO_EXT", true, ""}, {50, "GPIO_EXT", true, ""},
        {0, "Unverified", false, "Board pin unverified"}, {1, "Unverified", false, "Board pin unverified"},
        {54, "Port A SCL", false, "Port A I2C"}, {53, "Port A SDA", false, "Port A I2C"},
        {18, "M-Bus MOSI", true, ""}, {19, "M-Bus MISO", true, ""},
        {5, "M-Bus SCK", true, ""}, {38, "M-Bus RXD0", true, ""},
        {7, "M-Bus PC_RX", true, ""}, {3, "M-Bus GPIO", true, ""},
        {2, "M-Bus GPIO", true, ""}, {47, "M-Bus GPIO", true, ""},
        {16, "M-Bus GPIO", true, ""}, {17, "M-Bus PB_IN", true, ""},
        {45, "M-Bus GPIO", true, ""}, {52, "M-Bus PB_OUT", true, ""},
        {37, "M-Bus TXD0", true, ""}, {6, "M-Bus PC_TX", true, ""},
        {4, "M-Bus GPIO", true, ""}, {48, "M-Bus GPIO", true, ""},
        {35, "M-Bus GPIO", true, ""}, {51, "M-Bus GPIO", true, ""},
    };
}

inline bool gpioOutputAllowed(const std::vector<GpioPinInfo>& pins, uint8_t pin)
{
    const auto found = std::find_if(pins.begin(), pins.end(), [pin](const auto& info) { return info.pin == pin; });
    return found != pins.end() && found->outputAllowed;
}

}  // namespace hal

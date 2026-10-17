#pragma once
#include <cstdint>
#include <string>

namespace app_pps {

// Parse decimal setpoints exactly in thousandths before converting to the device's float protocol.
// Reject excess precision, exponent notation and out-of-range values instead of silently clamping.
inline bool parseSetpoint(const std::string& text, uint32_t minimumMilli, uint32_t maximumMilli, float& value)
{
    if (text.empty() || text.size() > 16) return false;
    uint32_t whole = 0, fraction = 0, digits = 0;
    bool decimal = false, hasWhole = false;
    for (char c : text) {
        if (c == '.' && !decimal && hasWhole) { decimal = true; continue; }
        if (c < '0' || c > '9') return false;
        if (decimal) {
            if (++digits > 3) return false;
            fraction = fraction * 10 + static_cast<uint32_t>(c - '0');
        } else {
            hasWhole = true;
            whole = whole * 10 + static_cast<uint32_t>(c - '0');
            if (whole > maximumMilli / 1000) return false;
        }
    }
    if (!hasWhole || (decimal && digits == 0)) return false;
    while (digits++ < 3) fraction *= 10;
    const uint32_t milli = whole * 1000 + fraction;
    if (milli < minimumMilli || milli > maximumMilli) return false;
    value = static_cast<float>(milli) / 1000.0f;
    return true;
}

}  // namespace app_pps

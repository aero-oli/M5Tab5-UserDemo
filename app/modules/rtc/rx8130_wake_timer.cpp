#include "rx8130_wake_timer.h"

namespace {
constexpr uint8_t counter = 0x1a, extension = 0x1c, flags = 0x1d, control = 0x1e;
}

bool rtc::Rx8130WakeTimer::read(uint8_t reg, uint8_t& value)
{
    if (_device.readRegister(reg, &value, 1)) return true;
    _error = "RTC wake timer read failed";
    return false;
}

bool rtc::Rx8130WakeTimer::write(uint8_t reg, uint8_t value)
{
    if (_device.writeRegister(reg, &value, 1)) return true;
    _error = "RTC wake timer write failed";
    return false;
}

bool rtc::Rx8130WakeTimer::cancel()
{
    _error.clear();
    uint8_t ext = 0, ctrl = 0, flag = 0;
    if (!read(extension, ext) || !read(control, ctrl) || !read(flags, flag)) return false;
    // Disable all wake IRQ sources, without changing clock/calendar or backup configuration.
    if (!write(extension, ext & ~0x10) || !write(control, ctrl & ~0x38) || !write(flags, flag & ~0x38)) return false;
    uint8_t check = 0;
    if (!read(control, check) || (check & 0x38) != 0) {
        _error = "RTC wake interrupt disable not verified";
        return false;
    }
    if (!read(extension, check) || (check & 0x10) != 0) {
        _error = "RTC wake timer disable not verified";
        return false;
    }
    return true;
}

bool rtc::Rx8130WakeTimer::arm(uint16_t seconds)
{
    _error.clear();
    if (seconds == 0) { _error = "RTC wake interval must be positive"; return false; }
    uint8_t ext = 0, ctrl = 0;
    if (!cancel() || !read(extension, ext) || !read(control, ctrl)) return false;
    if ((ctrl & 0xc0) != 0) { _error = "RTC clock is stopped or in test mode"; return false; }
    // Epson RX8130CE: TSEL=010 is 1 Hz; TBKE=0 counts on main and backup power.
    ext = (ext & ~0x17) | 0x02;
    ctrl = (ctrl & ~0x3d) | 0x10;
    const uint8_t preset[2] = {static_cast<uint8_t>(seconds), static_cast<uint8_t>(seconds >> 8)};
    uint8_t check[2] = {};
    if (!write(extension, ext) || !_device.writeRegister(counter, preset, 2) ||
        !_device.readRegister(counter, check, 2) || check[0] != preset[0] || check[1] != preset[1] ||
        !write(control, ctrl) || !write(extension, ext | 0x10)) {
        if (_error.empty()) _error = "RTC wake interval not verified";
        cancel();
        _error = "RTC wake timer configuration failed";
        return false;
    }
    uint8_t actualExt = 0, actualCtrl = 0;
    if (!read(extension, actualExt) || !read(control, actualCtrl) ||
        (actualExt & 0x17) != 0x12 || (actualCtrl & 0xfd) != (ctrl & 0xfd)) {
        cancel();
        _error = "RTC wake timer enable not verified";
        return false;
    }
    return true;
}

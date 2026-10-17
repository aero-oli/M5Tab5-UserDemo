#include "rx8130_calendar.h"
#include <array>
#include <algorithm>

namespace {
uint8_t bcd(int value)
{
    return static_cast<uint8_t>((value / 10) * 16 + value % 10);
}
bool decode(uint8_t value, int& result)
{
    if ((value & 15) > 9 || (value >> 4) > 9) return false;
    result = (value >> 4) * 10 + (value & 15);
    return true;
}
}  // namespace

bool rtc::Rx8130Calendar::readBytes(uint8_t reg, uint8_t* data, size_t count)
{
    if (_device.readRegister(reg, data, count)) return true;
    _error = "RTC read failed";
    return false;
}
bool rtc::Rx8130Calendar::writeBytes(uint8_t reg, const uint8_t* data, size_t count)
{
    if (_device.writeRegister(reg, data, count)) return true;
    _error = "RTC write failed; clock may be partially updated";
    return false;
}
bool rtc::Rx8130Calendar::writeVerified(uint8_t reg, const uint8_t* data, size_t count)
{
    std::array<uint8_t, 16> actual{};
    if (count > actual.size() || !writeBytes(reg, data, count) || !readBytes(reg, actual.data(), count)) return false;
    if (std::equal(data, data + count, actual.data())) return true;
    _error = "RTC initialization readback mismatch";
    return false;
}
bool rtc::Rx8130Calendar::read(tm& value, bool& needsInitialization)
{
    _error.clear();
    uint8_t flags = 0, control = 0;
    if (!readBytes(0x1d, &flags, 1) || !readBytes(0x1e, &control, 1)) return false;
    needsInitialization = (flags & 0x02) != 0 || (control & 0xc0) != 0;
    if (needsInitialization) {
        _error = (flags & 0x02) ? "RTC oscillator-invalid flag; initialization required"
                                : "RTC stopped or in test mode; initialization required";
        return false;
    }
    // One burst holds carry propagation for a coherent calendar without STOP.
    uint8_t data[7]{};
    if (!readBytes(0x10, data, sizeof(data))) return false;
    tm decoded{};
    int month = 0, year = 0;
    if (!decode(data[0], decoded.tm_sec) || !decode(data[1], decoded.tm_min) || !decode(data[2], decoded.tm_hour) ||
        !decode(data[4], decoded.tm_mday) || !decode(data[5], month) || !decode(data[6], year) || data[3] == 0 ||
        (data[3] & 0x80) || (data[3] & (data[3] - 1))) {
        _error = "RTC calendar register data invalid";
        return false;
    }
    decoded.tm_mon  = month - 1;
    decoded.tm_year = year + 100;
    if (!validCalendar(decoded) || data[3] != (1u << calendarWeekday(decoded))) {
        _error = "RTC calendar or weekday invalid";
        return false;
    }
    decoded.tm_wday  = calendarWeekday(decoded);
    decoded.tm_isdst = -1;
    // Catch oscillator-invalid flags raised during the calendar transaction.
    if (!readBytes(0x1d, &flags, 1)) return false;
    if (flags & 0x02) {
        needsInitialization = true;
        _error              = "RTC oscillator-invalid flag; initialization required";
        return false;
    }
    value = decoded;
    return true;
}
bool rtc::Rx8130Calendar::initializeRegisters()
{
    // Explicit recovery only. Known defaults for every user configuration/RAM
    // register, with wake/alarm/update IRQs disabled and backup switching enabled.
    // Epson ETM50E-10 sections 18.3/18.5: inhibit IRQs, establish backup
    // switching, initialize calendar/configuration, then clear VLF and STOP.
    // https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf
    // Tab5's RTC backup is a supercapacitor (M5Stack Specifications / RTC):
    // https://docs.m5stack.com/en/core/Tab5
    // Retain the board's original initBat policy: INIEN + CHGEN = 0x30.
    const uint8_t zero = 0, extension = 0x04, control = 0x40, backup = 0x30;
    const uint8_t alarmsAndTimer[] = {0x80, 0x80, 0x80, 0, 0};
    const uint8_t ram[]            = {0, 0, 0, 0};
    return writeVerified(0x1e, &control, 1) && writeVerified(0x30, &zero, 1) && writeVerified(0x31, &zero, 1) &&
           writeVerified(0x1c, &extension, 1) && writeVerified(0x1f, &backup, 1) &&
           writeVerified(0x17, alarmsAndTimer, sizeof(alarmsAndTimer)) && writeVerified(0x20, ram, sizeof(ram));
}
bool rtc::Rx8130Calendar::set(const tm& value, bool initialize, tm& actual)
{
    _error.clear();
    if (!validCalendar(value)) {
        _error = "Invalid date/time; use years 2000-2099";
        return false;
    }
    uint8_t flags = 0, control = 0;
    if (!readBytes(0x1d, &flags, 1) || !readBytes(0x1e, &control, 1)) return false;
    const bool invalid = (flags & 0x02) || (control & 0xc0);
    if (invalid && !initialize) {
        _error = "RTC initialization required; confirm clearing wake configuration";
        return false;
    }
    if (initialize && !initializeRegisters()) return false;
    const uint8_t calendar[] = {bcd(value.tm_sec),       bcd(value.tm_min),
                                bcd(value.tm_hour),      static_cast<uint8_t>(1u << calendarWeekday(value)),
                                bcd(value.tm_mday),      bcd(value.tm_mon + 1),
                                bcd(value.tm_year - 100)};
    // Epson explicitly permits setting without STOP. This preserves wake control
    // registers; writing SEC still resets the shared subsecond divider phase.
    if (!(initialize ? writeVerified(0x10, calendar, sizeof(calendar)) : writeBytes(0x10, calendar, sizeof(calendar))))
        return false;
    if (initialize) {
        // Acknowledge VLF only after all configuration/calendar writes succeeded.
        // Clear stale flags during explicit recovery, including old wake causes.
        const uint8_t clearFlags = 0;
        if (!writeVerified(0x1d, &clearFlags, 1)) return false;
        const uint8_t startClock = 0;
        if (!writeVerified(0x1e, &startClock, 1)) return false;
    }
    bool needsInitialization = false;
    if (!read(actual, needsInitialization)) return false;
    const auto elapsed = calendarSeconds(actual) - calendarSeconds(value);
    if (elapsed < 0 || elapsed > 1) {
        _error = "RTC set readback mismatch";
        return false;
    }
    return true;
}
bool rtc::Rx8130Calendar::clearWakeIrq()
{
    _error.clear();
    uint8_t flags = 0, control = 0;
    if (!readBytes(0x1d, &flags, 1) || !readBytes(0x1e, &control, 1)) return false;
    if (flags & 0x02) return true;  // No automatic initialization or acknowledgment of invalid data.
    // Flags are write-zero-to-clear. Write ones to preserve oscillator/backup/reset
    // indications even when an event arrives after the read above.
    const uint8_t preserveNonWake = 0x87;
    control &= ~0x38;
    return writeBytes(0x1d, &preserveNonWake, 1) && writeVerified(0x1e, &control, 1);
}

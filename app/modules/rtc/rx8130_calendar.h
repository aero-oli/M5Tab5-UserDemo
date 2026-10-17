#pragma once
#include <modules/i2c_device.h>
#include <hal/rtc_calendar.h>
#include <string>

namespace rtc {
// Epson RX8130CE ETM50E-10 sections 13.2, 14.1, 18.3, 18.5 and 18.6.
// Caller serializes complete calendar operations with wake-timer operations.
class Rx8130Calendar {
public:
    explicit Rx8130Calendar(modules::I2cDevice& device) : _device(device)
    {
    }
    bool read(tm& value, bool& needsInitialization);
    bool set(const tm& value, bool initialize, tm& actual);
    bool clearWakeIrq();  // Never acknowledges VLF or rewrites clock/calendar.
    const std::string& error() const
    {
        return _error;
    }

private:
    bool readBytes(uint8_t reg, uint8_t* data, size_t count);
    bool writeBytes(uint8_t reg, const uint8_t* data, size_t count);
    bool writeVerified(uint8_t reg, const uint8_t* data, size_t count);
    bool initializeRegisters();
    modules::I2cDevice& _device;
    std::string _error;
};
}  // namespace rtc

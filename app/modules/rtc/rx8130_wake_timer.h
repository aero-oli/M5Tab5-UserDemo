#pragma once
#include <cstdint>
#include <string>
#include <modules/i2c_device.h>

namespace rtc {
class Rx8130WakeTimer {
public:
    explicit Rx8130WakeTimer(modules::I2cDevice& device) : _device(device)
    {
    }
    bool arm(uint16_t seconds);
    bool cancel();
    const std::string& error() const
    {
        return _error;
    }

private:
    bool read(uint8_t reg, uint8_t& value);
    bool write(uint8_t reg, uint8_t value);
    modules::I2cDevice& _device;
    std::string _error;
};
}  // namespace rtc

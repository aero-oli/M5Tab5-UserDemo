#include <modules/rtc/rx8130_wake_timer.h>
#include <cassert>
#include <cstring>
#include <map>
#include <vector>

struct Device : modules::I2cDevice {
    std::map<uint8_t, std::vector<uint8_t>> data;
    int failure = -1, calls = 0;
    uint8_t ignoreWrite = 0xff;
    std::vector<uint8_t> writes;
    Device()
    {
        data[0x1c] = {0xa8};
        data[0x1d] = {0x3a};
        data[0x1e] = {0x22};
        data[0x1a] = {0, 0};
        data[0x10] = {0x49, 0x01, 0x12, 0x01, 0x11, 0x10, 0x26};
    }
    bool probe() override
    {
        return true;
    }
    uint8_t address() const override
    {
        return 0x32;
    }
    bool readRegister(uint8_t reg, uint8_t* out, size_t count) override
    {
        if (calls++ == failure || data[reg].size() != count) return false;
        std::memcpy(out, data[reg].data(), count);
        return true;
    }
    bool writeRegister(uint8_t reg, const uint8_t* value, size_t count) override
    {
        writes.push_back(reg);
        if (calls++ == failure) return false;
        if (reg != ignoreWrite) data[reg] = std::vector<uint8_t>(value, value + count);
        return true;
    }
};

int main()
{
    Device good;
    const auto clock = good.data[0x10];
    rtc::Rx8130WakeTimer timer(good);
    assert(!timer.arm(0) && good.calls == 0);
    assert(timer.arm(300));
    assert(good.data[0x1a] == std::vector<uint8_t>({0x2c, 0x01}));
    assert((good.data[0x1c][0] & 0x17) == 0x12);
    assert((good.data[0x1e][0] & 0x3d) == 0x10);
    assert(good.data[0x10] == clock);
    const auto transactions = good.calls;
    assert(timer.cancel());
    assert((good.data[0x1c][0] & 0x10) == 0 && (good.data[0x1e][0] & 0x38) == 0);
    for (auto reg : good.writes) assert(reg >= 0x1a && reg <= 0x1e);
    for (int failure = 0; failure < transactions; ++failure) {
        Device device;
        device.failure = failure;
        rtc::Rx8130WakeTimer t(device);
        assert(!t.arm(300));
        assert(!t.error().empty());
        assert(device.calls < 40 && device.data[0x10] == clock);
    }
    for (uint8_t ignored : {0x1a, 0x1c, 0x1e}) {
        Device device;
        device.ignoreWrite = ignored;
        rtc::Rx8130WakeTimer t(device);
        assert(!t.arm(300));
    }
    Device stopped;
    stopped.data[0x1e][0] |= 0x40;
    rtc::Rx8130WakeTimer invalid(stopped);
    assert(!invalid.arm(300));
    assert(invalid.error() == "RTC clock is stopped or in test mode");
}

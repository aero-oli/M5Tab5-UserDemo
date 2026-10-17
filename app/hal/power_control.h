#pragma once
#include <cstdint>
#include <string>

namespace hal {
enum class PowerAction { SleepTouch, SleepShake, SleepRtc, PowerOff };
struct PowerResult {
    bool success = false;
    bool displayAsleep = false;
    bool simulated = false;
    std::string message;
};
}

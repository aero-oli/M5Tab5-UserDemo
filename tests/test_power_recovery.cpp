#include <hal/power_recovery.h>
#include <cassert>
#include <string>
int main()
{
    int rtcCalls = 0, imuCalls = 0;
    auto run = [&](bool rtcOk, bool imuOk) {
        return hal::recoverPowerFailure(hal::PowerResult{false, false, false, "Audio stop failed"},
            [&] { ++rtcCalls; return rtcOk; }, [&] { ++imuCalls; return imuOk; });
    };
    auto result = run(true, true);
    assert(result.message == "Audio stop failed" && !result.success);
    result = run(false, false);
    assert(result.message.find("Audio stop failed") == 0);
    assert(result.message.find("RTC wake disable failed") != std::string::npos);
    assert(result.message.find("IMU normal-mode restore failed") != std::string::npos);
    assert(rtcCalls == 2 && imuCalls == 2); // Never short-circuit independent cleanup.
}

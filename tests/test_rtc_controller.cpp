#include <cassert>
#include <ctime>
#include <apps/app_hardware_rtc/rtc_controller.h>

int main()
{
    bool readCalled = false;
    app_hardware_rtc::RtcController controller([&](tm* out) {
        readCalled = true;
        out->tm_hour = 9;
        out->tm_min = 8;
        out->tm_sec = 7;
        out->tm_year = 126;
        out->tm_mon = 5;
        out->tm_mday = 6;
        out->tm_wday = 6;
    });

    auto sample = controller.sample();

    assert(readCalled);
    assert(app_hardware_rtc::RtcController::formatTime(sample) == "09:08:07");
    assert(app_hardware_rtc::RtcController::formatDate(sample) == "2026-06-06");
    assert(app_hardware_rtc::RtcController::formatWeekday(sample) == "Weekday: 6");

    return 0;
}

#include "../hal_desktop.h"
#include <hal/rtc_calendar.h>
#include <cstdlib>
#include <ctime>

void HalDesktop::updateRtcData()
{
    std::lock_guard<std::mutex> lock(_rtcIoMutex);
    auto data      = getRtcData();
    data.simulated = true;
    if (std::getenv("M5TAB5_DESKTOP_RTC_FAIL")) {
        data.valid = false;
        data.error = "Simulated RTC read failure";
    } else if (std::getenv("M5TAB5_DESKTOP_RTC_INVALID") && !_rtcSimulatedSet) {
        data.valid               = false;
        data.needsInitialization = true;
        data.error               = "Simulated oscillator-invalid flag; initialization required";
    } else {
        tm local{};
        if (_rtcSimulatedSet) {
            // Advance a wall-clock calendar without timezone/DST normalization.
            // The hardware has no timezone; setting it must preserve entered fields.
            const std::time_t epoch = static_cast<std::time_t>(946684800) + rtc::calendarSeconds(_rtcSimulatedTime) +
                                      (millis() - _rtcSimulatedAtMs) / 1000;
#ifdef _WIN32
            gmtime_s(&local, &epoch);
#else
            gmtime_r(&epoch, &local);
#endif
        } else {
            const auto now = std::time(nullptr);
#ifdef _WIN32
            localtime_s(&local, &now);
#else
            localtime_r(&now, &local);
#endif
        }
        data.valid               = rtc::validCalendar(local);
        data.needsInitialization = false;
        data.error               = data.valid ? "" : "Simulated host clock outside RTC range";
        if (data.valid) {
            data.time        = local;
            data.hasSample   = true;
            data.sampledAtMs = millis();
        }
    }
    publishRtcData(data);
}

bool HalDesktop::setRtcTimeChecked(const tm& time, bool initialize)
{
    std::lock_guard<std::mutex> lock(_rtcIoMutex);
    auto data      = getRtcData();
    data.simulated = true;
    data.valid     = false;
    if (!rtc::validCalendar(time))
        data.error = "Invalid date/time; use years 2000-2099";
    else if (std::getenv("M5TAB5_DESKTOP_RTC_FAIL"))
        data.error = "Simulated RTC write failure";
    else if (std::getenv("M5TAB5_DESKTOP_RTC_INVALID") && !_rtcSimulatedSet && !initialize) {
        data.needsInitialization = true;
        data.error               = "RTC initialization required; confirm clearing wake configuration";
    } else {
        _rtcSimulatedTime          = time;
        _rtcSimulatedTime.tm_isdst = -1;
        _rtcSimulatedAtMs          = millis();
        _rtcSimulatedSet           = true;
        data.time                  = time;
        data.time.tm_wday          = rtc::calendarWeekday(time);
        data.valid = data.hasSample = true;
        data.needsInitialization    = false;
        data.sampledAtMs            = millis();
        data.error.clear();
    }
    publishRtcData(data);
    return data.valid;
}

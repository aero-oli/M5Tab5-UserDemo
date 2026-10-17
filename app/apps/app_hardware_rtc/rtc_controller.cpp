#include "rtc_controller.h"
#include <hal/rtc_calendar.h>
#include <cstdio>
#include <algorithm>
#include <cctype>

using namespace app_hardware_rtc;
RtcController::RtcController(UpdateFn update, DataFn data, SetFn set)
    : _update(std::move(update)), _data(std::move(data)), _set(std::move(set))
{
}

RtcSample RtcController::cached() const
{
    std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->sample;
}
RtcSample RtcController::sample() const
{
    const auto state = _state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->sample.pending || !_update || !_data) return state->sample;
        state->sample.pending = true;
    }
    auto update = _update;
    auto data   = _data;
    app_runtime::AsyncJobRunner runner;
    if (!runner.start("RTC read", [state, update, data] {
            update();
            auto result = data();
            std::lock_guard<std::mutex> lock(state->mutex);
            static_cast<hal::HalBase::RtcData&>(state->sample) = std::move(result);
            state->sample.pending                              = false;
        })) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->sample.pending = false;
    }
    return cached();
}
bool RtcController::setClock(const tm& time, bool initialize)
{
    if (!rtc::validCalendar(time) || !_set || !_data) return false;
    const auto state = _state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->sample.pending) return false;
        state->sample.pending        = true;
        state->sample.commandMessage = "Setting clock...";
    }
    auto set  = _set;
    auto data = _data;
    app_runtime::AsyncJobRunner runner;
    if (!runner.start("RTC set", [state, set, data, time, initialize] {
            const bool ok = set(time, initialize);
            auto result   = data();
            std::lock_guard<std::mutex> lock(state->mutex);
            static_cast<hal::HalBase::RtcData&>(state->sample) = std::move(result);
            state->sample.pending                              = false;
            state->sample.commandMessage = ok ? "Clock set and readback verified" : "Clock set failed";
        })) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->sample.pending        = false;
        state->sample.commandMessage = "RTC worker unavailable";
        return false;
    }
    return true;
}
bool RtcController::parseEditor(const std::string& date, const std::string& time, tm& result)
{
    if (date.size() != 10 || time.size() != 8 || date[4] != '-' || date[7] != '-' || time[2] != ':' || time[5] != ':')
        return false;
    for (size_t i = 0; i < date.size(); ++i)
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(date[i]))) return false;
    for (size_t i = 0; i < time.size(); ++i)
        if (i != 2 && i != 5 && !std::isdigit(static_cast<unsigned char>(time[i]))) return false;
    tm parsed{};
    int year = 0, month = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &parsed.tm_mday) != 3 ||
        std::sscanf(time.c_str(), "%d:%d:%d", &parsed.tm_hour, &parsed.tm_min, &parsed.tm_sec) != 3)
        return false;
    parsed.tm_year  = year - 1900;
    parsed.tm_mon   = month - 1;
    parsed.tm_isdst = -1;
    if (!rtc::validCalendar(parsed)) return false;
    parsed.tm_wday = rtc::calendarWeekday(parsed);
    result         = parsed;
    return true;
}
std::string RtcController::statusText(const RtcSample& sample, uint32_t nowMs)
{
    if (!sample.valid) return sample.error.empty() ? "RTC unavailable" : sample.error;
    if (!sample.hasSample || nowMs - sample.sampledAtMs > 2000) return "RTC sample stale";
    return sample.simulated ? "Simulated clock" : "Hardware RTC";
}
std::string RtcController::formatTime(const RtcSample& sample)
{
    if (!sample.hasSample) return "--:--:--";
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", sample.time.tm_hour, sample.time.tm_min,
                  sample.time.tm_sec);
    return buffer;
}
std::string RtcController::formatDate(const RtcSample& sample)
{
    if (!sample.hasSample) return "---- -- --";
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", sample.time.tm_year + 1900, sample.time.tm_mon + 1,
                  sample.time.tm_mday);
    return buffer;
}
std::string RtcController::formatWeekday(const RtcSample& sample)
{
    static const char* names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    return sample.hasSample && sample.time.tm_wday >= 0 && sample.time.tm_wday < 7 ? names[sample.time.tm_wday]
                                                                                   : "Weekday unavailable";
}

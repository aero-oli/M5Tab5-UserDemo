#pragma once
#include <ctime>
#include <cstdint>

namespace rtc {
inline bool validCalendar(const tm& value)
{
    if (value.tm_year < 100 || value.tm_year > 199 || value.tm_mon < 0 || value.tm_mon > 11 ||
        value.tm_hour < 0 || value.tm_hour > 23 || value.tm_min < 0 || value.tm_min > 59 ||
        value.tm_sec < 0 || value.tm_sec > 59) return false;
    static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const int maxDay = days[value.tm_mon] + (value.tm_mon == 1 && value.tm_year % 4 == 0 ? 1 : 0);
    return value.tm_mday >= 1 && value.tm_mday <= maxDay;
}
// Range is deliberately 2000..2099: the chip stores only a two-digit year.
inline int64_t calendarSeconds(const tm& value)
{
    int days = 0;
    for (int year = 100; year < value.tm_year; ++year) days += year % 4 == 0 ? 366 : 365;
    static constexpr int monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int month = 0; month < value.tm_mon; ++month)
        days += monthDays[month] + (month == 1 && value.tm_year % 4 == 0 ? 1 : 0);
    days += value.tm_mday - 1;
    return ((static_cast<int64_t>(days) * 24 + value.tm_hour) * 60 + value.tm_min) * 60 + value.tm_sec;
}
inline int calendarWeekday(const tm& value)
{
    return static_cast<int>((calendarSeconds(value) / 86400 + 6) % 7); // 2000-01-01 was Saturday.
}
} // namespace rtc

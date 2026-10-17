#pragma once
#include <apps/app_registry.h>
#include <hal/hal.h>
#include <cstdio>

namespace launcher_view {
inline app_registry::AppStatus powerStatus(const hal::HalBase::PMData_t& data, uint32_t nowMs)
{
    using app_registry::StatusLevel;
    if (!data.hasSample) return {data.error.empty() ? "Not sampled" : "Read failed", StatusLevel::Warning};
    if (!data.valid) return {"Read failed; last good", StatusLevel::Warning};
    if (nowMs - data.sampledAtMs > 2000) return {"Stale reading", StatusLevel::Warning};
    char value[48];
    std::snprintf(value, sizeof(value), "%s%.2fV %.2fW", data.simulated ? "Sim " : "", data.busVoltage, data.busPower);
    return {value, data.simulated ? StatusLevel::Neutral : StatusLevel::Ok};
}

inline app_registry::AppStatus clockStatus(const hal::HalBase::RtcData& data, uint32_t nowMs)
{
    using app_registry::StatusLevel;
    if (data.needsInitialization) return {"Clock invalid", StatusLevel::Warning};
    if (!data.hasSample || !data.valid) return {"Clock unavailable", StatusLevel::Warning};
    if (nowMs - data.sampledAtMs > 2000) return {"Clock stale", StatusLevel::Warning};
    char value[24];
    std::snprintf(value, sizeof(value), "%s%02d:%02d", data.simulated ? "Sim " : "", data.time.tm_hour, data.time.tm_min);
    return {value, StatusLevel::Neutral};
}
} // namespace launcher_view

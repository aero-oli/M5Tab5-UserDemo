#pragma once
#include <hal/power_control.h>

namespace hal {
// Preserve the primary failure and report every failed rollback operation.
// Call both operations even if the first fails: wake sources are independent.
template <class CancelRtc, class RestoreImu>
PowerResult recoverPowerFailure(PowerResult result, CancelRtc cancelRtc, RestoreImu restoreImu)
{
    const bool rtcSafe = cancelRtc();
    const bool imuSafe = restoreImu();
    if (!rtcSafe) result.message += "; RTC wake disable failed";
    if (!imuSafe) result.message += "; IMU normal-mode restore failed";
    return result;
}
}  // namespace hal

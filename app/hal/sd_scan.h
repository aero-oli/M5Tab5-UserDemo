#pragma once
#include <hal/hal.h>
#include <utility>

namespace hal {

inline bool appendSdEntry(HalBase::SdScanResult& result, HalBase::FileEntry_t entry)
{
    if (result.entries.size() >= HalBase::kMaxSdEntries) {
        result.truncated = true;
        return false;
    }
    result.entries.push_back(std::move(entry));
    return true;
}

// One scan owns a temporary mount. A failed directory read still unmounts it;
// empty success is distinct from every failure path. Call only from a worker.
template <typename Mount, typename Read, typename Unmount>
HalBase::SdScanResult scanSdCardSession(Mount mount, Read read, Unmount unmount)
{
    auto result = mount();
    if (result.status != HalBase::SdScanStatus::Success) return result;
    result = read();
    if (!unmount()) {
        result.status = HalBase::SdScanStatus::ReadFailed;
        result.error += result.error.empty() ? "SD card cleanup failed" : "; SD card cleanup failed";
    }
    if (result.status != HalBase::SdScanStatus::Success) {
        result.entries.clear();
        result.truncated = false;
    }
    return result;
}

}  // namespace hal

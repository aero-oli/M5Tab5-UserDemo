#pragma once

namespace hal {
// A failed start can still leave the CSI controller running. Only a successful
// stop proves that the driver may release DMA buffers and completion semaphores.
template <class Stop, class Release>
bool releaseCameraCapture(bool startAttempted, Stop stop, Release release)
{
    if (startAttempted && !stop()) return false;
    release();
    return true;
}
}  // namespace hal

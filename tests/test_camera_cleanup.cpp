#include <hal/camera_cleanup.h>
#include <cassert>
#include <vector>
int main()
{
    std::vector<int> calls;
    auto stop = [&] {
        calls.push_back(1);
        return true;
    };
    auto release = [&] { calls.push_back(2); };
    assert(hal::releaseCameraCapture(false, stop, release));
    assert((calls == std::vector<int>{2}));  // No DMA start was attempted.
    calls.clear();
    assert(hal::releaseCameraCapture(true, stop, release));
    assert((calls == std::vector<int>{1, 2}));
    calls.clear();
    // Covers both failed STREAMON and failed STREAMOFF: ownership is uncertain.
    assert(!hal::releaseCameraCapture(
        true,
        [&] {
            calls.push_back(1);
            return false;
        },
        release));
    assert((calls == std::vector<int>{1}));  // Never unmap/close live DMA resources.
}

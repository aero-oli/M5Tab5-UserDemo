#include <apps/runtime/boot_health.h>
#include <cassert>
#include <cstdint>
using Gate = app_runtime::BootHealthGate;
int main()
{
    Gate healthy(0, 0, 0);
    assert(healthy.observe(50, 1, 1, true) == Gate::Decision::Waiting);
    assert(healthy.observe(2999, 180, 11, true) == Gate::Decision::Waiting);
    assert(healthy.observe(3000, 181, 12, true) == Gate::Decision::Accept);
    Gate late(0, 0, 0);
    assert(late.observe(10000, 600, 40, true) == Gate::Decision::Reject);
    Gate noRender(0, 0, 0);
    assert(noRender.observe(3000, 180, 1, true) == Gate::Decision::Waiting);
    assert(noRender.observe(10000, 600, 1, true) == Gate::Decision::Reject);
    Gate stalled(0, 0, 0);
    assert(stalled.observe(500, 100, 5, true) == Gate::Decision::Waiting);
    assert(stalled.observe(3000, 100, 5, true) == Gate::Decision::Waiting);
    assert(stalled.observe(10000, 100, 5, true) == Gate::Decision::Reject);
    Gate missingCore(0, 0, 0);
    assert(missingCore.observe(3000, 180, 12, false) == Gate::Decision::Waiting);
    assert(missingCore.observe(10000, 600, 40, false) == Gate::Decision::Reject);
    Gate sparse(0, 0, 0);
    assert(sparse.observe(3000, 2, 12, true) == Gate::Decision::Waiting);
    assert(sparse.observe(10000, 3, 40, true) == Gate::Decision::Reject);
    Gate reset(0, 100, 10);
    assert(reset.observe(2000, 200, 20, true) == Gate::Decision::Waiting);
    assert(reset.observe(2500, 1, 1, true) == Gate::Decision::Waiting);
    assert(reset.observe(3000, 100, 5, true) == Gate::Decision::Waiting);
    assert(reset.observe(5500, 280, 13, true) == Gate::Decision::Accept);
    Gate lateCore(0, 0, 0);
    assert(lateCore.observe(2500, 150, 10, false) == Gate::Decision::Waiting);
    assert(lateCore.observe(3000, 180, 12, true) == Gate::Decision::Waiting);
    assert(lateCore.observe(5500, 330, 22, true) == Gate::Decision::Accept);
    constexpr uint32_t start = UINT32_MAX - 1000;
    Gate wrapping(start, 400, 30);
    assert(wrapping.observe(start + 2999, 580, 41, true) == Gate::Decision::Waiting);
    assert(wrapping.observe(start + 3000, 581, 42, true) == Gate::Decision::Accept);
}

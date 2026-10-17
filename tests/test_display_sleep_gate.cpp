#include <hal/display_sleep_gate.h>
#include <cassert>

int main()
{
    hal::DisplaySleepGate gate;
    assert(!gate.filter(true).consume);
    gate.arm(true);
    assert(gate.asleep());
    assert(!gate.filter(true).wake);
    assert(!gate.filter(false).wake);
    assert(gate.filter(true).wake);
    assert(!gate.asleep());
    assert(gate.filter(true).consume && !gate.filter(true).wake);
    assert(gate.filter(false).consume);
    assert(!gate.filter(true).consume);
    gate.arm(false);
    assert(gate.filter(false).consume);
    assert(gate.filter(true).wake);
    assert(gate.filter(false).consume);
    assert(!gate.filter(false).consume);
    gate.arm(false);
    gate.wake();
    assert(!gate.asleep() && gate.filter(true).consume);
    assert(gate.filter(false).consume && !gate.filter(true).consume);
}

#pragma once

namespace hal {

// Filter the existing input stream; never poll the touch controller from a second task.
class DisplaySleepGate {
public:
    struct Sample { bool consume; bool wake; };
    void arm(bool pressed) { _stage = pressed ? Stage::WaitRelease : Stage::WaitPress; }
    bool asleep() const { return _stage == Stage::WaitRelease || _stage == Stage::WaitPress; }
    void wake() { if (asleep()) _stage = Stage::WakeHeld; }
    Sample filter(bool pressed)
    {
        switch (_stage) {
            case Stage::WaitRelease:
                if (!pressed) _stage = Stage::WaitPress;
                return {true, false};
            case Stage::WaitPress:
                if (pressed) { _stage = Stage::WakeHeld; return {true, true}; }
                return {true, false};
            case Stage::WakeHeld:
                if (!pressed) _stage = Stage::Awake;
                return {true, false};
            case Stage::Awake:
                return {false, false};
        }
        return {false, false};
    }
private:
    enum class Stage { Awake, WaitRelease, WaitPress, WakeHeld };
    Stage _stage = Stage::Awake;
};
}

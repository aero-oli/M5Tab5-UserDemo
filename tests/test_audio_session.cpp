#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include <hal/audio_session.h>
#include <apps/runtime/async_job_runner.h>

using Operation = hal::HalBase::AudioOperation;
using Phase     = hal::HalBase::AudioPhase;

int main()
{
    // Match the BSP: volume zero mutes but preserves the preceding audible gain.
    hal::AudioVolume output;
    int gain        = 60;
    bool muted      = false;
    int volumeCalls = 0;
    auto setVolume  = [&](uint8_t value) {
        ++volumeCalls;
        if (value == 0)
            muted = true;
        else {
            gain  = value;
            muted = false;
        }
        return true;
    };
    auto setMute = [&](bool value) {
        muted = value;
        return true;
    };
    assert(output.apply(60, setVolume, setMute));
    assert(!muted && gain == 60);
    assert(output.apply(0, setVolume, setMute));
    assert(muted && gain == 60);
    // Repeated transfer boundaries cannot unmute the stale positive gain.
    assert(output.apply(0, setVolume, setMute));
    assert(muted && volumeCalls == 2);
    assert(output.apply(25, setVolume, setMute));
    assert(!muted && gain == 25);
    assert(output.mute(setMute));
    assert(muted);
    assert(output.apply(25, setVolume, setMute));
    assert(!muted && gain == 25);
    output.invalidate();  // Clock changes can restore the codec's stored mute.
    assert(!output.apply(
        0, [](uint8_t) { return false; }, setMute));
    assert(output.apply(0, setVolume, setMute));
    assert(muted);

    hal::AudioSession session;
    auto token = session.reserve(Operation::DualMic, Phase::Recording);
    assert(token);
    assert(!session.reserve(Operation::Tone, Phase::Playing));
    assert(!session.reserve(Operation::Music, Phase::Playing));
    session.cancel();
    assert(session.cancelled(token));
    session.setPhase(token, Phase::Playing);
    assert(session.snapshot().phase == Phase::Cancelling);
    // Back cannot release ownership while a driver call still uses the buffer.
    assert(!session.stopAndWait(0));
    auto status = session.snapshot();
    assert(status.busy && status.phase == Phase::Failed && !status.lastError.empty());
    assert(!session.reserve(Operation::Tone, Phase::Playing));
    session.finish(token);
    assert(!session.snapshot().busy);
    assert(session.stopAndWait(0));
    assert(!session.reserve(Operation::Tone, Phase::Playing));  // Shutdown admission remains closed.
    session.resume();

    const auto next = session.reserve(Operation::HeadphoneMic, Phase::Recording);
    assert(next && next != token);
    session.finish(token);  // A stale worker must never release a new session.
    assert(session.snapshot().busy);
    session.startFailed(next);
    status = session.snapshot();
    assert(!status.busy && status.phase == Phase::Failed && !status.lastError.empty());
    token = session.reserve(Operation::Tone, Phase::Playing);
    assert(token);
    std::vector<int16_t> buffer(10000);
    size_t calls = 0;
    assert(hal::transferAudio(session, token, buffer.data(), buffer.size() * sizeof(int16_t),
                              [&](void*, size_t bytes, size_t& done) {
                                  assert(bytes <= 4096);
                                  ++calls;
                                  done = bytes / 2;  // Valid partial progress must be retried.
                                  if (done % 2) ++done;
                                  return true;
                              }));
    assert(calls > 5);
    session.finish(token);

    for (int fault = 0; fault < 4; ++fault) {
        token = session.reserve(Operation::Record, Phase::Recording);
        assert(!hal::transferAudio(session, token, buffer.data(), 4096, [&](void*, size_t bytes, size_t& done) {
            done = fault == 1 ? bytes + 2 : fault == 2 ? 1 : 0;
            return fault != 3;
        }));
        assert(!session.snapshot().lastError.empty());
        session.finish(token);
    }

    token = session.reserve(Operation::DualMic, Phase::Recording);
    calls = 0;
    assert(!hal::transferAudio(session, token, buffer.data(), buffer.size() * sizeof(int16_t),
                               [&](void*, size_t bytes, size_t& done) {
                                   ++calls;
                                   session.cancel();
                                   done = bytes;
                                   return true;
                               }));
    assert(calls == 1 && session.snapshot().lastError.empty());
    session.finish(token);

    // Real registered-runner admission failure uses the same production rollback.
    app_runtime::AsyncJobRunner runner;
    assert(app_runtime::AsyncJobRunner::shutdownAll(1000));
    token = session.reserve(Operation::Music, Phase::Starting);
    assert(!runner.start("audio", [] {}));
    session.startFailed(token);
    assert(!session.snapshot().busy && session.snapshot().phase == Phase::Failed);
    assert(app_runtime::AsyncJobRunner::resumeAll());

    token = session.reserve(Operation::Music, Phase::Playing);
    std::atomic<bool> entered{false};
    assert(runner.start("audio cancellation", [&] {
        entered.store(true);
        while (!session.cancelled(token)) std::this_thread::yield();
        session.finish(token);
    }));
    while (!entered.load()) std::this_thread::yield();
    assert(session.stopAndWait(1000));
    assert(app_runtime::AsyncJobRunner::waitForAll(1000));
    assert(!session.snapshot().busy);
    session.resume();
    return 0;
}

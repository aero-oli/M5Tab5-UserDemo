#include <hal/uart_monitor.h>
#include <cassert>
#include <future>
#include <thread>
#include <vector>

using State = hal::UartMonitorState;
using namespace std::chrono;

void test_whole_message_bounds_and_rx_retention()
{
    State state;
    assert(state.queueTx(std::string(State::kCapacity - 2, 'a'), false));
    assert(!state.queueTx("bc", true));  // No partial message or newline admitted.
    assert(state.txQueue.size() == State::kCapacity - 2);
    assert(state.queueTx("b", true));
    assert(!state.queueTx("c", false));
    assert(state.txQueue.size() == State::kCapacity);
    std::vector<uint8_t> queued(State::kCapacity);
    assert(state.takeTx(queued.data(), queued.size()) == queued.size());
    assert(queued[queued.size() - 2] == 'b' && queued.back() == '\n');
    assert(!state.queueTx(std::string(State::kCapacity, 'a'), true));
    const std::string input(State::kCapacity + 17, 'x');
    state.appendRx(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    assert(state.rxQueue.size() == State::kCapacity && state.rxDropped == 17);
    state.requestStop();
    assert(!state.queueTx("after shutdown", false));
    assert(state.waitForWorker(0));
}

void test_partial_and_zero_writes_preserve_order()
{
    auto state = std::make_shared<State>();
    hal::UartMonitorWorker worker(state);
    assert(state->queueTx("abcdef", false));
    std::string output;
    int writes       = 0;
    const auto read  = [](uint8_t*, size_t) { return 0; };
    const auto write = [&](const uint8_t* bytes, size_t size) {
        if (++writes == 1) return 0;  // FIFO backpressure must not discard pending data.
        const auto count = std::min<size_t>(2, size);
        output.append(reinterpret_cast<const char*>(bytes), count);
        return static_cast<int>(count);
    };
    assert(worker.poll(read, write));
    assert(output.empty());
    assert(state->queueTx("gh", false));
    for (int i = 0; i < 4; ++i) assert(worker.poll(read, write));
    assert(output == "abcdefgh");
    state->requestStop();
    assert(!worker.poll(read, write));
}

void test_blocked_io_does_not_lock_snapshots_and_cleanup_precedes_exit()
{
    auto state           = std::make_shared<State>();
    state->workerRunning = state->driverActive = true;
    assert(state->queueTx("before", false));
    std::weak_ptr<State> lifetime = state;
    std::promise<void> writing, releaseWrite, cleaning, releaseCleanup;
    auto writeGate = releaseWrite.get_future().share(), cleanupGate = releaseCleanup.get_future().share();
    auto job = std::async(std::launch::async, [owned = state, &writing, writeGate, &cleaning, cleanupGate] {
        hal::UartMonitorWorker worker(owned);
        worker.poll([](uint8_t*, size_t) { return 0; },
                    [&](const uint8_t*, size_t size) {
                        writing.set_value();
                        writeGate.wait();
                        return static_cast<int>(size);
                    });
        cleaning.set_value();
        cleanupGate.wait();
        owned->finishWorker(true);
    });
    assert(writing.get_future().wait_for(seconds(2)) == std::future_status::ready);
    auto reader = std::async(std::launch::async, [&] {
        // Same queue operations used by UI remain available during hardware IO.
        assert(state->queueTx("during", false));
        const uint8_t byte = 'r';
        state->appendRx(&byte, 1);
        state->requestStop();
    });
    assert(reader.wait_for(milliseconds(100)) == std::future_status::ready);
    reader.get();
    assert(!state->waitForWorker(1));
    releaseWrite.set_value();
    assert(cleaning.get_future().wait_for(seconds(2)) == std::future_status::ready);
    assert(!state->waitForWorker(1));  // Finished IO is not finished driver cleanup.
    auto observer = state;
    state.reset();
    assert(!lifetime.expired());  // Releasing the HAL owner cannot destroy worker storage.
    releaseCleanup.set_value();
    assert(job.wait_for(seconds(2)) == std::future_status::ready);
    job.get();
    assert(observer->waitForWorker(1) && !observer->driverActive);
    observer.reset();
    assert(lifetime.expired());
}

void test_error_and_failed_cleanup_remain_visible()
{
    auto state = std::make_shared<State>();
    hal::UartMonitorWorker worker(state);
    assert(!worker.poll([](uint8_t*, size_t) { return -1; }, [](const uint8_t*, size_t) { return 0; }));
    assert(state->error == "UART receive failed" && !state->queueTx("x", false));
    state->finishWorker(false);
    assert(state->waitForWorker(0) && state->driverActive && state->error == "UART driver cleanup failed");
    state->finishWorker(true);
    assert(!state->driverActive);
}

void test_desktop_echo_is_immediate_bounded_and_owned()
{
    State state;
    assert(state.echo("test", true));
    std::string text;
    while (!state.rxQueue.empty()) {
        text.push_back(static_cast<char>(state.rxQueue.front()));
        state.rxQueue.pop();
    }
    assert(text == "[simulated echo] test\n" && state.simulated);
    assert(state.echo(std::string(State::kCapacity, 'x'), false));
    assert(state.rxQueue.size() == State::kCapacity && state.rxDropped > 0);
    assert(!state.echo(std::string(State::kCapacity, 'x'), true));
    state.requestStop();
    assert(!state.echo("after shutdown", false));
}

int main()
{
    test_whole_message_bounds_and_rx_retention();
    test_partial_and_zero_writes_preserve_order();
    test_blocked_io_does_not_lock_snapshots_and_cleanup_precedes_exit();
    test_error_and_failed_cleanup_remain_visible();
    test_desktop_echo_is_immediate_bounded_and_owned();
}

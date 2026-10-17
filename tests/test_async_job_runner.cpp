#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#include <apps/runtime/async_job_runner.h>

using namespace std::chrono_literals;

static void wait_for_idle()
{
    auto& runner = app_runtime::globalAsyncJobRunner();
    for (int i = 0; i < 100; ++i) {
        if (!runner.busy()) {
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    assert(false && "async job runner did not become idle");
}

static void test_runs_job_and_clears_busy()
{
    auto& runner = app_runtime::globalAsyncJobRunner();
    wait_for_idle();

    std::atomic<int> calls{0};
    assert(runner.start("unit test", [&]() { calls.fetch_add(1); }));
    wait_for_idle();

    assert(calls.load() == 1);
    assert(!runner.busy());
    assert(runner.currentJob().empty());
}

static void test_rejects_concurrent_jobs()
{
    auto& runner = app_runtime::globalAsyncJobRunner();
    wait_for_idle();

    std::atomic<bool> release{false};
    std::atomic<int> secondCalls{0};
    assert(runner.start("long unit test", [&]() {
        while (!release.load()) {
            std::this_thread::sleep_for(5ms);
        }
    }));
    assert(runner.busy());
    assert(!runner.start("should not run", [&]() { secondCalls.fetch_add(1); }));

    release.store(true);
    wait_for_idle();
    assert(secondCalls.load() == 0);
}

static void test_runner_lifetime_and_shutdown_admission()
{
    std::atomic<bool> release{false};
    std::atomic<bool> complete{false};
    {
        app_runtime::AsyncJobRunner runner;
        assert(runner.start("outlives owner", [&] {
            while (!release.load()) std::this_thread::sleep_for(1ms);
            complete = true;
        }));
    }
    assert(!app_runtime::AsyncJobRunner::shutdownAll(5));
    assert(!app_runtime::AsyncJobRunner::resumeAll());
    app_runtime::AsyncJobRunner other;
    assert(!other.start("after shutdown", [] {}));
    release = true;
    assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    assert(complete);
    assert(app_runtime::AsyncJobRunner::resumeAll());
}

int main()
{
    test_runs_job_and_clears_busy();
    test_rejects_concurrent_jobs();
    test_runner_lifetime_and_shutdown_admission();
    assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    return 0;
}

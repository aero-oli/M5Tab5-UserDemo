/* SPDX-License-Identifier: MIT */
#include "async_job_runner.h"
#include <chrono>
#include <condition_variable>
#include <new>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <thread>
#include <system_error>
#endif

using namespace app_runtime;

struct AsyncJobRunner::State {
    std::mutex mutex;
    bool busy = false;
    std::string currentJob;
};

namespace {
struct Jobs {
    std::mutex mutex;
    std::condition_variable changed;
    size_t active  = 0;
    bool accepting = true;
};
Jobs& jobs()
{
    static Jobs value;
    return value;
}

#if defined(ESP_PLATFORM)
void runJob(void* arg)
{
    auto* work = static_cast<std::function<void()>*>(arg);
    (*work)();
    delete work;  // FreeRTOS self-deletion does not unwind C++ locals.
    vTaskDelete(nullptr);
}
#endif
}  // namespace

AsyncJobRunner::AsyncJobRunner() : _state(std::make_shared<State>())
{
}

bool AsyncJobRunner::start(const char* name, std::function<void()> job)
{
    if (!job) return false;
    auto state = _state;
    {
        auto& all = jobs();
        std::lock_guard<std::mutex> admission(all.mutex);
        if (!all.accepting) return false;
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->busy) return false;
        state->busy       = true;
        state->currentJob = name ? name : "background job";
        ++all.active;
    }
    // Capture shared state, never the runner. Views may release a runner during work.
    auto work = [state, job = std::move(job)]() mutable {
        job();
        job = {};  // Release callback captures before announcing dependency-safe completion.
        finish(state);
    };
#if defined(ESP_PLATFORM)
    auto* task = new (std::nothrow) std::function<void()>(std::move(work));
    if (!task || xTaskCreate(runJob, "app_async_job", 8192, task, 4, nullptr) != pdPASS) {
        delete task;
        finish(state);
        return false;
    }
#else
    try {
        std::thread(std::move(work)).detach();
    } catch (const std::system_error&) {
        finish(state);
        return false;
    }
#endif
    return true;
}

bool AsyncJobRunner::busy() const
{
    std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->busy;
}
std::string AsyncJobRunner::currentJob() const
{
    std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->currentJob;
}
void AsyncJobRunner::finish(const std::shared_ptr<State>& state)
{
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->currentJob.clear();
        state->busy = false;
    }
    auto& all = jobs();
    std::lock_guard<std::mutex> lock(all.mutex);
    --all.active;
    all.changed.notify_all();
}
bool AsyncJobRunner::waitForAll(uint32_t timeoutMs)
{
    auto& all = jobs();
    std::unique_lock<std::mutex> lock(all.mutex);
    return all.changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return all.active == 0; });
}
bool AsyncJobRunner::shutdownAll(uint32_t timeoutMs)
{
    auto& all = jobs();
    {
        std::lock_guard<std::mutex> lock(all.mutex);
        all.accepting = false;
    }
    return waitForAll(timeoutMs);
}
bool AsyncJobRunner::resumeAll()
{
    auto& all = jobs();
    std::lock_guard<std::mutex> lock(all.mutex);
    if (all.active) return false;
    all.accepting = true;
    return true;
}
size_t AsyncJobRunner::activeJobs()
{
    auto& all = jobs();
    std::lock_guard<std::mutex> lock(all.mutex);
    return all.active;
}
AsyncJobRunner& app_runtime::globalAsyncJobRunner()
{
    static AsyncJobRunner runner;
    return runner;
}

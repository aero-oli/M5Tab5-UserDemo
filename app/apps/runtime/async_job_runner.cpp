/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "async_job_runner.h"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <thread>
#endif

using namespace app_runtime;

namespace {

#if defined(ESP_PLATFORM)
struct FreeRtosJob {
    std::function<void()> job;
    std::function<void()> done;
};

void run_free_rtos_job(void* arg)
{
    auto* work = static_cast<FreeRtosJob*>(arg);
    if (work != nullptr) {
        if (work->job) {
            work->job();
        }
        if (work->done) {
            work->done();
        }
        delete work;
    }
    vTaskDelete(nullptr);
}
#endif

}  // namespace

bool AsyncJobRunner::start(const char* name, std::function<void()> job)
{
    if (!job) {
        return false;
    }

    bool expected = false;
    if (!_busy.compare_exchange_strong(expected, true)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _currentJob = name == nullptr ? "background job" : name;
    }

#if defined(ESP_PLATFORM)
    auto* work = new FreeRtosJob {std::move(job), [this]() { finish(); }};
    if (xTaskCreate(run_free_rtos_job, "app_async_job", 8192, work, 4, nullptr) != pdPASS) {
        delete work;
        finish();
        return false;
    }
#else
    std::thread([this, job = std::move(job)]() mutable {
        job();
        finish();
    }).detach();
#endif

    return true;
}

bool AsyncJobRunner::busy() const
{
    return _busy.load();
}

std::string AsyncJobRunner::currentJob() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _currentJob;
}

void AsyncJobRunner::finish()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _currentJob.clear();
    }
    _busy.store(false);
}

AsyncJobRunner& app_runtime::globalAsyncJobRunner()
{
    static AsyncJobRunner runner;
    return runner;
}

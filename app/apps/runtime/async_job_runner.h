/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace app_runtime {

class AsyncJobRunner {
public:
    AsyncJobRunner();
    AsyncJobRunner(const AsyncJobRunner&) = delete;
    AsyncJobRunner& operator=(const AsyncJobRunner&) = delete;
    bool start(const char* name, std::function<void()> job);
    bool busy() const;
    std::string currentJob() const;

    // Shutdown stops admission and drains every runner, including app-local samplers.
    // A timeout never kills a worker or permits its dependencies to be destroyed.
    static bool shutdownAll(uint32_t timeoutMs);
    static bool waitForAll(uint32_t timeoutMs);
    static bool resumeAll();
    static size_t activeJobs();

private:
    struct State;
    std::shared_ptr<State> _state;
    static void finish(const std::shared_ptr<State>& state);
};

AsyncJobRunner& globalAsyncJobRunner();

}  // namespace app_runtime

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace app_runtime {

class AsyncJobRunner {
public:
    bool start(const char* name, std::function<void()> job);
    bool busy() const;
    std::string currentJob() const;

private:
    void finish();

    std::atomic<bool> _busy {false};
    mutable std::mutex _mutex;
    std::string _currentJob;
};

AsyncJobRunner& globalAsyncJobRunner();

}  // namespace app_runtime

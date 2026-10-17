/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "async_job_runner.h"
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace app_runtime {

// Workers own their result storage; neither a blocked read nor controller destruction
// holds the snapshot mutex or requires the worker to access the controller.
template <typename Sample>
class AsyncSample {
public:
    Sample request(const char* name, std::function<Sample()> read) const
    {
        const auto state = _state;
        Sample snapshot;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->value.pending || !read) {
                return state->value;
            }
            state->value.pending = true;
            snapshot             = state->value;
        }
        if (!_runner->start(name, [state, read = std::move(read)] {
                auto value = read();
                // A completed read may be unavailable or stale. Its health metadata
                // belongs to the sampler, not to background-job completion.
                value.pending = false;
                std::lock_guard<std::mutex> lock(state->mutex);
                state->value = value;
            })) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->value.pending = false;
            snapshot             = state->value;
        }
        return snapshot;
    }

    Sample cached() const
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        return _state->value;
    }

private:
    struct State {
        std::mutex mutex;
        Sample value;
    };
    std::shared_ptr<State> _state           = std::make_shared<State>();
    std::shared_ptr<AsyncJobRunner> _runner = std::make_shared<AsyncJobRunner>();
};

}  // namespace app_runtime

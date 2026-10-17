#pragma once
#include <hal/hal.h>
#include <apps/runtime/async_job_runner.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace app_hardware_rtc {
struct RtcSample : hal::HalBase::RtcData {
    bool pending = false;
    std::string commandMessage;
};
class RtcController {
public:
    using UpdateFn = std::function<void()>;
    using DataFn   = std::function<hal::HalBase::RtcData()>;
    using SetFn    = std::function<bool(const tm&, bool)>;
    RtcController(UpdateFn update, DataFn data, SetFn set);
    RtcSample sample() const;  // Dispatch a read; never wait on I/O.
    RtcSample cached() const;
    bool setClock(const tm& time, bool initialize);
    static bool parseEditor(const std::string& date, const std::string& time, tm& result);
    static std::string statusText(const RtcSample& sample, uint32_t nowMs);
    static std::string formatTime(const RtcSample& sample);
    static std::string formatDate(const RtcSample& sample);
    static std::string formatWeekday(const RtcSample& sample);

private:
    struct State {
        std::mutex mutex;
        RtcSample sample;
    };
    UpdateFn _update;
    DataFn _data;
    SetFn _set;
    std::shared_ptr<State> _state = std::make_shared<State>();
};
}  // namespace app_hardware_rtc

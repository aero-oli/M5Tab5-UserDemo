#include <apps/app_hardware_rtc/rtc_controller.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
using namespace app_hardware_rtc;
using namespace std::chrono_literals;
int main()
{
    tm parsed{};
    assert(RtcController::parseEditor("2024-02-29","23:59:58",parsed));
    assert(parsed.tm_mon==1 && parsed.tm_wday==4);
    for (auto bad : {"2025-02-29","2026-04-31","2100-01-01","1999-12-31","2026-1-01","2026-01-01x"})
        assert(!RtcController::parseEditor(bad,"09:08:07",parsed));
    for (auto bad : {"24:00:00","12:60:00","12:00:60","1:02:03","12:00:00x"})
        assert(!RtcController::parseEditor("2026-01-01",bad,parsed));
    RtcSample empty;
    assert(RtcController::formatTime(empty)=="--:--:--");
    std::mutex mutex; std::condition_variable changed;
    bool entered=false, release=false;
    hal::HalBase::RtcData data;
    std::atomic<int> writes{0};
    RtcController controller([&] {
        std::unique_lock<std::mutex> lock(mutex); entered=true; changed.notify_all();
        changed.wait(lock,[&] { return release; });
        data.valid=data.hasSample=true; data.sampledAtMs=10;
        assert(RtcController::parseEditor("2026-06-06","09:08:07",data.time));
        data.error.clear();
    }, [&] { return data; }, [&](const tm& chosen,bool initialize) {
        ++writes; data.time=chosen; data.valid=true; data.needsInitialization=false; return true;
    });
    const auto start=std::chrono::steady_clock::now();
    assert(controller.sample().pending);
    assert(std::chrono::steady_clock::now()-start < 100ms);
    { std::unique_lock<std::mutex> lock(mutex); assert(changed.wait_for(lock,1s,[&]{return entered;})); }
    assert(controller.cached().pending && !controller.cached().hasSample);
    assert(!controller.setClock(parsed,false)); // No concurrent write during read.
    { std::lock_guard<std::mutex> lock(mutex); release=true; changed.notify_all(); }
    assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    auto sample=controller.cached();
    assert(RtcController::formatTime(sample)=="09:08:07");
    assert(RtcController::formatDate(sample)=="2026-06-06");
    assert(RtcController::formatWeekday(sample)=="Saturday");
    assert(RtcController::statusText(sample,20)=="Hardware RTC");
    assert(RtcController::statusText(sample,2011)=="RTC sample stale");
    assert(RtcController::parseEditor("2024-02-29","23:59:58",parsed));
    assert(controller.setClock(parsed,true));
    assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    assert(writes==1 && controller.cached().commandMessage=="Clock set and readback verified");
    data.valid=false; data.error="RTC transport failed";
    RtcController failed([]{},[&]{return data;},[](const tm&,bool){return false;});
    failed.sample(); assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    sample=failed.cached();
    assert(sample.hasSample && !sample.valid);
    assert(RtcController::statusText(sample,20)=="RTC transport failed");
    assert(failed.setClock(parsed,false));
    assert(app_runtime::AsyncJobRunner::waitForAll(2000));
    assert(failed.cached().commandMessage=="Clock set failed");
    assert(!failed.cached().valid);
}

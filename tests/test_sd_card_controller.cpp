#include <cassert>
#include <future>
#include <memory>
#include <apps/app_hardware_sd/sd_card_controller.h>
#include <apps/runtime/async_job_runner.h>
#include <hal/sd_scan.h>

using Hal = hal::HalBase;
using Status = Hal::SdScanStatus;
using Result = Hal::SdScanResult;
using Controller = app_hardware_sd::SdCardController;

void test_result_semantics()
{
    assert(Controller::formatResult({Status::Success, {}, {}}) == "SD card root is empty.");
    assert(Controller::formatResult({Status::Success, {{"firmware.bin", false}, {"logs", true}}, {}}) ==
           "SD card root:\n      firmware.bin\n[DIR] logs");
    for (auto status : {Status::MountFailed, Status::ReadFailed, Status::Unavailable}) {
        const auto text = Controller::formatResult({status, {}, "Injected failure"});
        assert(text.find("Injected failure") != std::string::npos);
        assert(text.find("empty") == std::string::npos);
    }
    assert(Controller::formatResult({Status::Success, {}, {}, true}).find("Desktop simulation") != std::string::npos);
    Result capped{Status::Success, {}, {}};
    for (size_t index = 0; index < Hal::kMaxSdEntries; ++index) assert(hal::appendSdEntry(capped, {"file", false}));
    assert(!capped.truncated);
    assert(!hal::appendSdEntry(capped, {"excess", false}));
    assert(capped.truncated && capped.entries.size() == Hal::kMaxSdEntries);
    assert(Controller::formatResult(capped).find("Showing first 256 entries") != std::string::npos);
}

void test_mounted_session_cleanup()
{
    int reads = 0, cleanups = 0;
    auto mount = [] { return Result{Status::Success, {}, {}}; };
    auto cleanup = [&] { ++cleanups; return true; };
    auto result = hal::scanSdCardSession(
        [] { return Result{Status::MountFailed, {}, "No usable card"}; },
        [&] { ++reads; return Result{Status::Success, {}, {}}; }, cleanup);
    assert(result.status == Status::MountFailed && reads == 0 && cleanups == 0);
    result = hal::scanSdCardSession(mount, [&] { ++reads; return Result{Status::Success, {}, {}}; }, cleanup);
    assert(result.status == Status::Success && result.entries.empty() && reads == 1 && cleanups == 1);
    result = hal::scanSdCardSession(mount, [] { return Result{Status::ReadFailed, {}, "Open failed"}; }, cleanup);
    assert(result.status == Status::ReadFailed && result.error == "Open failed" && cleanups == 2);
    result = hal::scanSdCardSession(mount, [] { return Result{Status::ReadFailed, {{"partial", false}}, "Read failed"}; }, cleanup);
    assert(result.status == Status::ReadFailed && result.entries.empty() && cleanups == 3);
    result = hal::scanSdCardSession(mount, [] { return Result{Status::Success, {{"file", false}}, {}}; }, [] { return false; });
    assert(result.status == Status::ReadFailed && result.entries.empty() && result.error == "SD card cleanup failed");
}

void test_async_error_and_destruction()
{
    using namespace std::chrono;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    auto controller = std::make_unique<Controller>([&entered, gate](const std::string&) {
        entered.set_value();
        gate.wait();
        return Result{Status::ReadFailed, {}, "Injected failure"};
    });
    assert(controller->startScan());
    assert(entered.get_future().wait_for(seconds(2)) == std::future_status::ready);
    assert(controller->busy());
    const auto scanning = controller->statusText();
    assert(!controller->startScan());
    assert(controller->busy() && controller->statusText() == scanning);
    controller.reset();
    release.set_value();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    Controller failed([](const std::string&) { return Result{Status::MountFailed, {}, "Injected failure"}; });
    assert(failed.startScan());
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    failed.refresh();
    assert(!failed.busy() && failed.statusText().find("mount failed") != std::string::npos);
}

void test_duplicate_scan_preserves_the_pending_result()
{
    using namespace std::chrono;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    int calls = 0;
    Controller controller([&](const std::string&) {
        ++calls;
        entered.set_value();
        gate.wait();
        return Result{Status::Success, {{"retained-result.bin", false}}, {}};
    });
    assert(controller.startScan());
    assert(entered.get_future().wait_for(seconds(2)) == std::future_status::ready);
    assert(!controller.startScan());
    assert(controller.busy() && controller.statusText() == "Scanning SD card...");
    release.set_value();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    // Even after the global worker finishes, local ownership lasts until refresh.
    assert(!controller.startScan() && controller.busy());
    controller.refresh();
    assert(!controller.busy() && calls == 1);
    assert(controller.statusText().find("retained-result.bin") != std::string::npos);
}

int main()
{
    test_result_semantics();
    test_mounted_session_cleanup();
    test_async_error_and_destruction();
    test_duplicate_scan_preserves_the_pending_result();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
}

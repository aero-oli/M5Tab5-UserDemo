#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <apps/app_hardware_imu/imu_controller.h>

int main()
{
    using namespace std::chrono;
    auto count = std::make_shared<int>(0);
    std::promise<void> entered, release, readDone;
    auto gate = release.get_future().share();
    hal::HalBase::IMUData_t data;
    data.accelX = 0.1234f;
    data.accelY = -0.5f;
    data.accelZ = 1.0f;
    data.gyroX = 12.25f;
    auto controller = std::make_unique<app_hardware_imu::ImuController>(
        [count, &entered, gate] { if (++*count == 2) { entered.set_value(); gate.wait(); } },
        [data, count, &readDone] { if (*count == 2) { readDone.set_value(); } return data; });
    const auto first = controller->sample();
    assert(!first.hasSample && first.pending);
    auto deadline = steady_clock::now() + seconds(2);
    while (!controller->cached().hasSample) {
        assert(steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    auto sample = controller->cached();
    assert(sample.accelX == data.accelX && sample.gyroX == data.gyroX);
    assert(app_hardware_imu::ImuController::formatAccel(sample.accelX) == "0.123 g");
    assert(app_hardware_imu::ImuController::formatAccel(sample.accelY) == "-0.500 g");
    assert(app_hardware_imu::ImuController::formatGyro(sample.gyroX) == "12.25 dps");
    auto started = entered.get_future();
    deadline = steady_clock::now() + seconds(2);
    while (started.wait_for(milliseconds(0)) != std::future_status::ready) {
        assert(steady_clock::now() < deadline);
        controller->sample();
        std::this_thread::yield();
    }
    auto close = std::async(std::launch::async, [&] {
        for (int i = 0; i < 100; ++i) {
            const auto cached = controller->sample();
            assert(cached.hasSample && cached.pending && cached.accelX == data.accelX);
        }
        controller.reset();
    });
    assert(close.wait_for(milliseconds(100)) == std::future_status::ready);
    close.get();
    // Reopening uses independent result storage while the old read is outstanding.
    app_hardware_imu::ImuController reopened([] {}, [data] { return data; });
    assert(!reopened.sample().hasSample);
    release.set_value();
    assert(readDone.get_future().wait_for(seconds(2)) == std::future_status::ready);
    deadline = steady_clock::now() + seconds(2);
    while (!reopened.cached().hasSample) {
        assert(steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    assert(reopened.cached().accelX == data.accelX);
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    return 0;
}

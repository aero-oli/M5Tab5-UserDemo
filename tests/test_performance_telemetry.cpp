#include <cassert>

#include <apps/runtime/performance_telemetry.h>

static void test_records_loop_samples_and_slow_frames()
{
    app_runtime::PerformanceTelemetry telemetry(50, 4);

    telemetry.recordAppUpdate(0, 12);
    telemetry.recordAppUpdate(20, 96);

    auto snapshot = telemetry.snapshot();
    assert(snapshot.totalAppUpdates == 2);
    assert(snapshot.lastAppUpdateMs == 76);
    assert(snapshot.maxAppUpdateMs == 76);
    assert(snapshot.slowAppUpdates == 1);
    assert(snapshot.avgAppUpdateMs == 44);
}

static void test_average_is_bounded_to_recent_samples()
{
    app_runtime::PerformanceTelemetry telemetry(50, 3);

    telemetry.recordAppUpdate(0, 10);
    telemetry.recordAppUpdate(10, 20);
    telemetry.recordAppUpdate(20, 30);
    telemetry.recordAppUpdate(30, 90);

    auto snapshot = telemetry.snapshot();
    assert(snapshot.totalAppUpdates == 4);
    assert(snapshot.avgAppUpdateMs == 26);
    assert(snapshot.maxAppUpdateMs == 60);
    assert(snapshot.slowAppUpdates == 1);
}

static void test_background_job_state_is_cached()
{
    app_runtime::PerformanceTelemetry telemetry;

    telemetry.recordBackgroundJob(true, "Wi-Fi scan");
    auto busy = telemetry.snapshot();
    assert(busy.backgroundBusy);
    assert(busy.backgroundJob == "Wi-Fi scan");

    telemetry.recordBackgroundJob(false, "");
    auto idle = telemetry.snapshot();
    assert(!idle.backgroundBusy);
    assert(idle.backgroundJob.empty());
}

static void test_device_metrics_and_input_samples()
{
    app_runtime::PerformanceTelemetry telemetry;
    telemetry.recordRender(1000, 1500);
    telemetry.recordLock(80, 900);
    telemetry.recordLock(10, 30);
    telemetry.recordFlush(50);  // No input: not a latency sample.
    for (unsigned i = 1; i <= 100; ++i) {
        telemetry.recordInput(1000000);
        telemetry.recordInput(1000001);  // Preserve earliest pending press.
        telemetry.recordFlush(1000000 + i * 1000);
    }
    telemetry.recordResources(10000, 9000, 2048, 12);
    const auto value = telemetry.snapshot();
    assert(value.renderedFrames == 1 && value.lastRenderUs == 500);
    assert(value.maxLockWaitUs == 80 && value.maxLockHoldUs == 900);
    assert(value.inputSamples == 100 && value.p95InputToFlushUs == 95000);
    assert(value.maxInputToFlushUs == 100000 && value.minimumHeap == 9000);
    telemetry.reset();
    assert(telemetry.snapshot().inputSamples == 0);
}

int main()
{
    test_records_loop_samples_and_slow_frames();
    test_average_is_bounded_to_recent_samples();
    test_background_job_state_is_cached();
    test_device_metrics_and_input_samples();
    return 0;
}

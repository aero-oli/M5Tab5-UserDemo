#include <cassert>

#include "apps/runtime/update_scheduler.h"

static void test_first_update_runs_immediately()
{
    app_runtime::UpdateScheduler scheduler(16);

    assert(scheduler.shouldRun(100));
    assert(!scheduler.shouldRun(100));
}

static void test_updates_are_rate_limited()
{
    app_runtime::UpdateScheduler scheduler(16);

    assert(scheduler.shouldRun(10));
    assert(!scheduler.shouldRun(25));
    assert(scheduler.shouldRun(26));
}

static void test_zero_interval_disables_rate_limit()
{
    app_runtime::UpdateScheduler scheduler(0);

    assert(scheduler.shouldRun(10));
    assert(scheduler.shouldRun(10));
    assert(scheduler.shouldRun(11));
}

static void test_timer_wrap_is_handled()
{
    app_runtime::UpdateScheduler scheduler(16);

    assert(scheduler.shouldRun(0xFFFFFFF0u));
    assert(!scheduler.shouldRun(0xFFFFFFFEu));
    assert(scheduler.shouldRun(0x00000000u));
}

int main()
{
    test_first_update_runs_immediately();
    test_updates_are_rate_limited();
    test_zero_interval_disables_rate_limit();
    test_timer_wrap_is_handled();
    return 0;
}

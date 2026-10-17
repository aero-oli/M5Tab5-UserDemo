#include <cassert>
#include <memory>
#include <string>

#include "apps/app_registry.h"
#include <mooncake.h>

class RegistryTestApp : public mooncake::AppAbility {
public:
    explicit RegistryTestApp(const std::string& name)
    {
        setAppInfo().name = name;
    }
};

static void test_registry_installs_and_orders_apps()
{
    app_registry::AppRegistry registry;

    registry.add({app_registry::AppKey::Rtc,
                  app_registry::AppCategory::Hardware,
                  "RTC",
                  "Clock and date",
                  "12:00",
                  "RT",
                  20,
                  80,
                  false,
                  true,
                  [] { return std::make_unique<RegistryTestApp>("RTC"); },
                  [] { return app_registry::AppStatus{"12:34", app_registry::StatusLevel::Ok}; }});
    registry.add({app_registry::AppKey::PowerMonitor,
                  app_registry::AppCategory::Hardware,
                  "Power",
                  "Battery and input",
                  "7.9V",
                  "PW",
                  10,
                  100,
                  false,
                  true,
                  [] { return std::make_unique<RegistryTestApp>("Power"); },
                  [] { return app_registry::AppStatus{"8.0V", app_registry::StatusLevel::Ok}; }});

    registry.installAll();

    const auto& all = registry.all();
    assert(all.size() == 2);
    assert(all[0].key == app_registry::AppKey::PowerMonitor);
    assert(all[1].key == app_registry::AppKey::Rtc);
    assert(all[0].icon == "PW");
    assert(all[0].summaryPriority == 100);
    assert(!all[0].dangerous);
    assert(all[0].dashboardVisible);
    assert(registry.find(app_registry::AppKey::Rtc) != nullptr);
    assert(registry.find(app_registry::AppKey::Rtc)->appId >= 0);

    auto hardware = registry.byCategory(app_registry::AppCategory::Hardware);
    assert(hardware.size() == 2);
    assert(hardware[0]->label == "Power");
    assert(hardware[1]->label == "RTC");

    auto status = registry.status(app_registry::AppKey::Rtc);
    assert(status.text == "12:34");
    assert(status.level == app_registry::StatusLevel::Ok);
}

static void test_registry_open_rejects_missing_key()
{
    app_registry::AppRegistry registry;
    assert(!registry.open(app_registry::AppKey::Pps));
}

static void test_dashboard_apps_are_filtered_and_priority_sorted()
{
    app_registry::AppRegistry registry;
    registry.add({app_registry::AppKey::Music,
                  app_registry::AppCategory::Media,
                  "Music",
                  "Playback",
                  "Idle",
                  "MU",
                  10,
                  5,
                  false,
                  false,
                  [] { return std::make_unique<RegistryTestApp>("Music"); }});
    registry.add({app_registry::AppKey::Pps,
                  app_registry::AppCategory::Modules,
                  "PPS",
                  "Supply",
                  "0x35",
                  "PS",
                  10,
                  100,
                  false,
                  true,
                  [] { return std::make_unique<RegistryTestApp>("PPS"); }});
    registry.add({app_registry::AppKey::PowerMonitor,
                  app_registry::AppCategory::Hardware,
                  "Power",
                  "Power data",
                  "8V",
                  "PW",
                  10,
                  120,
                  false,
                  true,
                  [] { return std::make_unique<RegistryTestApp>("Power"); }});

    auto dashboard = registry.dashboardApps();
    assert(dashboard.size() == 2);
    assert(dashboard[0]->key == app_registry::AppKey::PowerMonitor);
    assert(dashboard[1]->key == app_registry::AppKey::Pps);
}

static void test_logical_categories_are_ordered()
{
    const auto& cats = app_registry::categories();
    assert(cats.size() == 5);
    assert(cats[0] == app_registry::AppCategory::Home);
    assert(cats[1] == app_registry::AppCategory::Settings);
    assert(cats[2] == app_registry::AppCategory::Hardware);
    assert(cats[3] == app_registry::AppCategory::Media);
    assert(cats[4] == app_registry::AppCategory::Modules);
    assert(std::string(app_registry::categoryName(app_registry::AppCategory::Home)) == "Home");
}

int main()
{
    test_registry_installs_and_orders_apps();
    test_registry_open_rejects_missing_key();
    test_dashboard_apps_are_filtered_and_priority_sorted();
    test_logical_categories_are_ordered();
    mooncake::DestroyMooncake();
    return 0;
}

cmake_minimum_required(VERSION 3.10)
project(MC_APP_BOILERPLATE VERSION 0.1.0 LANGUAGES C CXX)

include(platforms/desktop/CMakeLists.txt)

enable_testing()

add_test(NAME desktop_runtime_smoke_tests
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 $<TARGET_FILE:app_desktop_build>
)
set_tests_properties(desktop_runtime_smoke_tests PROPERTIES TIMEOUT 20)

add_test(NAME desktop_network_empty_ui_tests
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_WIFI_NO_NETWORKS=1 $<TARGET_FILE:app_desktop_build>
)
add_test(NAME desktop_network_failure_ui_tests
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_WIFI_CONNECT_FAIL=1 $<TARGET_FILE:app_desktop_build>
)
set_tests_properties(desktop_network_empty_ui_tests desktop_network_failure_ui_tests PROPERTIES TIMEOUT 20)

foreach(camera_fault start frame stall)
    add_test(NAME desktop_camera_${camera_fault}_ui_tests
        COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_CAMERA_FAIL=${camera_fault} $<TARGET_FILE:app_desktop_build>
    )
    set_tests_properties(desktop_camera_${camera_fault}_ui_tests PROPERTIES TIMEOUT 20)
endforeach()

add_executable(pps_device_tests
    tests/test_pps_device.cpp
    app/modules/pps/pps_device.cpp
)
target_include_directories(pps_device_tests PUBLIC app)
target_include_directories(pps_device_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(pps_device_tests PUBLIC cxx_std_17)
add_test(NAME pps_device_tests COMMAND pps_device_tests)

add_executable(pps_controller_tests
    tests/test_pps_controller.cpp
    app/apps/app_pps/pps_controller.cpp
    app/apps/runtime/async_job_runner.cpp
    app/modules/pps/pps_device.cpp
    app/modules/i2c_device.cpp
)
target_include_directories(pps_controller_tests PUBLIC app)
target_include_directories(pps_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(pps_controller_tests PUBLIC cxx_std_17)
target_link_libraries(pps_controller_tests PUBLIC mooncake_log)
add_test(NAME pps_controller_tests COMMAND pps_controller_tests)
set_tests_properties(pps_controller_tests PROPERTIES TIMEOUT 20)

add_test(NAME desktop_pps_absent_ui_tests
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_PPS_ABSENT=1 $<TARGET_FILE:app_desktop_build>
)
set_tests_properties(desktop_pps_absent_ui_tests PROPERTIES TIMEOUT 20)

foreach(gpio_fault init set release)
    add_test(NAME desktop_gpio_${gpio_fault}_ui_tests
        COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_GPIO_FAIL=${gpio_fault} $<TARGET_FILE:app_desktop_build>
    )
    set_tests_properties(desktop_gpio_${gpio_fault}_ui_tests PROPERTIES TIMEOUT 20)
endforeach()

add_test(NAME desktop_power_failure_ui_tests
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy M5TAB5_DESKTOP_MAX_FRAMES=8 M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI=1 M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI=1 M5TAB5_DESKTOP_POWER_FAIL=1 $<TARGET_FILE:app_desktop_build>
)
set_tests_properties(desktop_power_failure_ui_tests PROPERTIES TIMEOUT 20)

add_executable(app_registry_tests
    tests/test_app_registry.cpp
    app/apps/app_registry.cpp
)
target_include_directories(app_registry_tests PUBLIC app dependencies/mooncake/src)
target_include_directories(app_registry_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(app_registry_tests PUBLIC cxx_std_17)
target_link_libraries(app_registry_tests PUBLIC mooncake mooncake_log)
add_test(NAME app_registry_tests COMMAND app_registry_tests)

add_executable(app_catalog_metadata_tests
    tests/test_app_catalog_metadata.cpp
)
target_compile_features(app_catalog_metadata_tests PUBLIC cxx_std_17)
add_test(NAME app_catalog_metadata_tests COMMAND app_catalog_metadata_tests)

add_executable(ui_layout_tests
    tests/test_ui_layout.cpp
)
target_include_directories(ui_layout_tests PUBLIC app)
target_compile_features(ui_layout_tests PUBLIC cxx_std_17)
add_test(NAME ui_layout_tests COMMAND ui_layout_tests)

add_executable(app_shell_layout_tests
    tests/test_app_shell_layout.cpp
)
target_include_directories(app_shell_layout_tests PUBLIC app)
target_compile_features(app_shell_layout_tests PUBLIC cxx_std_17)
add_test(NAME app_shell_layout_tests COMMAND app_shell_layout_tests)

add_executable(async_job_runner_tests
    tests/test_async_job_runner.cpp
    app/apps/runtime/async_job_runner.cpp
)
target_include_directories(async_job_runner_tests PUBLIC app)
target_compile_features(async_job_runner_tests PUBLIC cxx_std_17)
target_link_libraries(async_job_runner_tests PUBLIC pthread)
add_test(NAME async_job_runner_tests COMMAND async_job_runner_tests)

add_executable(performance_telemetry_tests
    tests/test_performance_telemetry.cpp
    app/apps/runtime/performance_telemetry.cpp
)
target_include_directories(performance_telemetry_tests PUBLIC app)
target_compile_features(performance_telemetry_tests PUBLIC cxx_std_17)
add_test(NAME performance_telemetry_tests COMMAND performance_telemetry_tests)

add_executable(update_scheduler_tests
    tests/test_update_scheduler.cpp
)
target_include_directories(update_scheduler_tests PUBLIC app)
target_compile_features(update_scheduler_tests PUBLIC cxx_std_17)
add_test(NAME update_scheduler_tests COMMAND update_scheduler_tests)

add_executable(ui_architecture_guard_tests
    tests/test_ui_architecture_guard.cpp
)
target_compile_features(ui_architecture_guard_tests PUBLIC cxx_std_17)
add_test(NAME ui_architecture_guard_tests COMMAND ui_architecture_guard_tests)

add_executable(i2c_scan_controller_tests
    tests/test_i2c_scan_controller.cpp
    app/apps/app_hardware_i2c/i2c_scan_controller.cpp
    app/apps/runtime/async_job_runner.cpp
)
target_include_directories(i2c_scan_controller_tests PUBLIC app)
target_compile_features(i2c_scan_controller_tests PUBLIC cxx_std_17)
add_test(NAME i2c_scan_controller_tests COMMAND i2c_scan_controller_tests)

add_executable(sd_card_controller_tests
    tests/test_sd_card_controller.cpp
    app/apps/app_hardware_sd/sd_card_controller.cpp
    app/apps/runtime/async_job_runner.cpp
)
target_include_directories(sd_card_controller_tests PUBLIC app)
target_include_directories(sd_card_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(sd_card_controller_tests PUBLIC cxx_std_17)
add_test(NAME sd_card_controller_tests COMMAND sd_card_controller_tests)

add_executable(gpio_controller_tests
    tests/test_gpio_controller.cpp
    app/apps/app_hardware_gpio/gpio_controller.cpp
)
target_include_directories(gpio_controller_tests PUBLIC app)
target_compile_features(gpio_controller_tests PUBLIC cxx_std_17)
add_test(NAME gpio_controller_tests COMMAND gpio_controller_tests)

add_executable(power_monitor_controller_tests
    tests/test_power_monitor_controller.cpp
    app/apps/app_hardware_power_monitor/power_monitor_controller.cpp
)
target_include_directories(power_monitor_controller_tests PUBLIC app)
target_include_directories(power_monitor_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(power_monitor_controller_tests PUBLIC cxx_std_17)
add_test(NAME power_monitor_controller_tests COMMAND power_monitor_controller_tests)

add_executable(switches_controller_tests
    tests/test_switches_controller.cpp
    app/apps/app_hardware_switches/switches_controller.cpp
)
target_include_directories(switches_controller_tests PUBLIC app)
target_compile_features(switches_controller_tests PUBLIC cxx_std_17)
add_test(NAME switches_controller_tests COMMAND switches_controller_tests)

add_executable(display_sound_controller_tests
    tests/test_display_sound_controller.cpp
    app/apps/app_settings_display_sound/display_sound_controller.cpp
)
target_include_directories(display_sound_controller_tests PUBLIC app)
target_compile_features(display_sound_controller_tests PUBLIC cxx_std_17)
add_test(NAME display_sound_controller_tests COMMAND display_sound_controller_tests)

add_executable(power_settings_controller_tests
    tests/test_power_settings_controller.cpp
    app/apps/app_settings_power/power_settings_controller.cpp
    app/apps/runtime/async_job_runner.cpp
)
target_include_directories(power_settings_controller_tests PUBLIC app)
target_include_directories(power_settings_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(power_settings_controller_tests PUBLIC cxx_std_17)
add_test(NAME power_settings_controller_tests COMMAND power_settings_controller_tests)
set_tests_properties(power_settings_controller_tests PROPERTIES TIMEOUT 10)

add_executable(rtc_wake_timer_tests tests/test_rtc_wake_timer.cpp app/modules/rtc/rx8130_wake_timer.cpp)
target_include_directories(rtc_wake_timer_tests PUBLIC app)
target_include_directories(rtc_wake_timer_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(rtc_wake_timer_tests PUBLIC cxx_std_17)
add_test(NAME rtc_wake_timer_tests COMMAND rtc_wake_timer_tests)

add_executable(display_sleep_gate_tests tests/test_display_sleep_gate.cpp)
target_include_directories(display_sleep_gate_tests PUBLIC app)
target_compile_features(display_sleep_gate_tests PUBLIC cxx_std_17)
add_test(NAME display_sleep_gate_tests COMMAND display_sleep_gate_tests)

add_executable(rtc_controller_tests
    tests/test_rtc_controller.cpp
    app/apps/app_hardware_rtc/rtc_controller.cpp
)
target_include_directories(rtc_controller_tests PUBLIC app)
target_include_directories(rtc_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(rtc_controller_tests PUBLIC cxx_std_17)
add_test(NAME rtc_controller_tests COMMAND rtc_controller_tests)

add_executable(imu_controller_tests
    tests/test_imu_controller.cpp
    app/apps/app_hardware_imu/imu_controller.cpp
)
target_include_directories(imu_controller_tests PUBLIC app)
target_include_directories(imu_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(imu_controller_tests PUBLIC cxx_std_17)
add_test(NAME imu_controller_tests COMMAND imu_controller_tests)

add_executable(com_monitor_controller_tests
    tests/test_com_monitor_controller.cpp
    app/apps/app_hardware_com_monitor/com_monitor_controller.cpp
)
target_include_directories(com_monitor_controller_tests PUBLIC app)
target_include_directories(com_monitor_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(com_monitor_controller_tests PUBLIC cxx_std_17)
add_test(NAME com_monitor_controller_tests COMMAND com_monitor_controller_tests)

add_executable(audio_tool_controller_tests
    tests/test_audio_tool_controller.cpp
    app/apps/app_media_audio/audio_tool_controller.cpp
)
target_include_directories(audio_tool_controller_tests PUBLIC app)
target_include_directories(audio_tool_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(audio_tool_controller_tests PUBLIC cxx_std_17)
add_test(NAME audio_tool_controller_tests COMMAND audio_tool_controller_tests)

add_executable(camera_controller_tests
    tests/test_camera_controller.cpp
    app/apps/app_media_camera/camera_controller.cpp
)
target_include_directories(camera_controller_tests PUBLIC app)
target_include_directories(camera_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(camera_controller_tests PUBLIC cxx_std_17)
add_test(NAME camera_controller_tests COMMAND camera_controller_tests)

add_executable(network_settings_controller_tests
    tests/test_network_settings_controller.cpp
    app/apps/app_settings_network/network_controller.cpp
)
target_include_directories(network_settings_controller_tests PUBLIC app)
target_include_directories(network_settings_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(network_settings_controller_tests PUBLIC cxx_std_17)
add_test(NAME network_settings_controller_tests COMMAND network_settings_controller_tests)

add_executable(updates_controller_tests
    tests/test_updates_controller.cpp
    app/apps/app_settings_updates/updates_controller.cpp
)
target_include_directories(updates_controller_tests PUBLIC app)
target_include_directories(updates_controller_tests SYSTEM PUBLIC dependencies/lvgl)
target_compile_features(updates_controller_tests PUBLIC cxx_std_17)
add_test(NAME updates_controller_tests COMMAND updates_controller_tests)

add_executable(pps_layout_tests
    tests/test_pps_layout.cpp
)
target_include_directories(pps_layout_tests PUBLIC app)
target_compile_features(pps_layout_tests PUBLIC cxx_std_17)
add_test(NAME pps_layout_tests COMMAND pps_layout_tests)

add_executable(launcher_layout_tests
    tests/test_launcher_layout.cpp
)
target_include_directories(launcher_layout_tests PUBLIC app)
target_compile_features(launcher_layout_tests PUBLIC cxx_std_17)
add_test(NAME launcher_layout_tests COMMAND launcher_layout_tests)

add_executable(gpio_layout_tests
    tests/test_gpio_layout.cpp
)
target_include_directories(gpio_layout_tests PUBLIC app)
target_compile_features(gpio_layout_tests PUBLIC cxx_std_17)
add_test(NAME gpio_layout_tests COMMAND gpio_layout_tests)

add_executable(i2c_scan_layout_tests
    tests/test_i2c_scan_layout.cpp
)
target_include_directories(i2c_scan_layout_tests PUBLIC app)
target_compile_features(i2c_scan_layout_tests PUBLIC cxx_std_17)
add_test(NAME i2c_scan_layout_tests COMMAND i2c_scan_layout_tests)

add_executable(sd_card_layout_tests
    tests/test_sd_card_layout.cpp
)
target_include_directories(sd_card_layout_tests PUBLIC app)
target_compile_features(sd_card_layout_tests PUBLIC cxx_std_17)
add_test(NAME sd_card_layout_tests COMMAND sd_card_layout_tests)

add_executable(com_monitor_layout_tests
    tests/test_com_monitor_layout.cpp
)
target_include_directories(com_monitor_layout_tests PUBLIC app)
target_compile_features(com_monitor_layout_tests PUBLIC cxx_std_17)
add_test(NAME com_monitor_layout_tests COMMAND com_monitor_layout_tests)

add_executable(audio_tool_layout_tests
    tests/test_audio_tool_layout.cpp
)
target_include_directories(audio_tool_layout_tests PUBLIC app)
target_compile_features(audio_tool_layout_tests PUBLIC cxx_std_17)
add_test(NAME audio_tool_layout_tests COMMAND audio_tool_layout_tests)

add_executable(display_sound_layout_tests
    tests/test_display_sound_layout.cpp
)
target_include_directories(display_sound_layout_tests PUBLIC app)
target_compile_features(display_sound_layout_tests PUBLIC cxx_std_17)
add_test(NAME display_sound_layout_tests COMMAND display_sound_layout_tests)

add_executable(power_settings_layout_tests
    tests/test_power_settings_layout.cpp
)
target_include_directories(power_settings_layout_tests PUBLIC app)
target_compile_features(power_settings_layout_tests PUBLIC cxx_std_17)
add_test(NAME power_settings_layout_tests COMMAND power_settings_layout_tests)

add_executable(rtc_layout_tests
    tests/test_rtc_layout.cpp
)
target_include_directories(rtc_layout_tests PUBLIC app)
target_compile_features(rtc_layout_tests PUBLIC cxx_std_17)
add_test(NAME rtc_layout_tests COMMAND rtc_layout_tests)

add_executable(imu_layout_tests
    tests/test_imu_layout.cpp
)
target_include_directories(imu_layout_tests PUBLIC app)
target_compile_features(imu_layout_tests PUBLIC cxx_std_17)
add_test(NAME imu_layout_tests COMMAND imu_layout_tests)

add_executable(power_monitor_layout_tests
    tests/test_power_monitor_layout.cpp
)
target_include_directories(power_monitor_layout_tests PUBLIC app)
target_compile_features(power_monitor_layout_tests PUBLIC cxx_std_17)
add_test(NAME power_monitor_layout_tests COMMAND power_monitor_layout_tests)

add_executable(about_layout_tests
    tests/test_about_layout.cpp
)
target_include_directories(about_layout_tests PUBLIC app)
target_compile_features(about_layout_tests PUBLIC cxx_std_17)
add_test(NAME about_layout_tests COMMAND about_layout_tests)

add_executable(diagnostics_layout_tests
    tests/test_diagnostics_layout.cpp
)
target_include_directories(diagnostics_layout_tests PUBLIC app)
target_compile_features(diagnostics_layout_tests PUBLIC cxx_std_17)
add_test(NAME diagnostics_layout_tests COMMAND diagnostics_layout_tests)

add_executable(switches_layout_tests
    tests/test_switches_layout.cpp
)
target_include_directories(switches_layout_tests PUBLIC app)
target_compile_features(switches_layout_tests PUBLIC cxx_std_17)
add_test(NAME switches_layout_tests COMMAND switches_layout_tests)

add_executable(camera_layout_tests
    tests/test_camera_layout.cpp
)
target_include_directories(camera_layout_tests PUBLIC app)
target_compile_features(camera_layout_tests PUBLIC cxx_std_17)
add_test(NAME camera_layout_tests COMMAND camera_layout_tests)

add_executable(network_layout_tests
    tests/test_network_layout.cpp
)
target_include_directories(network_layout_tests PUBLIC app)
target_compile_features(network_layout_tests PUBLIC cxx_std_17)
add_test(NAME network_layout_tests COMMAND network_layout_tests)

add_executable(updates_layout_tests
    tests/test_updates_layout.cpp
)
target_include_directories(updates_layout_tests PUBLIC app)
target_compile_features(updates_layout_tests PUBLIC cxx_std_17)
add_test(NAME updates_layout_tests COMMAND updates_layout_tests)

# Included by the root project; each configure preset owns its build directory.
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DPLATFORM_BUILD_DESKTOP)

# Mooncake 
set(MOONCAKE_BUILD_EXAMPLE OFF)
add_subdirectory(dependencies/mooncake)

# Mooncake log
set(MOONCAKE_LOG_BUILD_EXAMPLE OFF)
add_subdirectory(dependencies/mooncake_log)

# Smooth UI toolkit
set(SMOOTH_UI_TOOLKIT_BUILD_EXAMPLE OFF)
set(SMOOTH_UI_TOOLKIT_BUILD_TEST OFF)
add_subdirectory(dependencies/smooth_ui_toolkit)

# Lvgl
set(LV_CONF_INCLUDE_SIMPLE OFF)
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h")
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON)
set(LV_CONF_BUILD_DISABLE_DEMOS ON)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON)
add_subdirectory(dependencies/lvgl)

# SDL
find_package(SDL2 REQUIRED)
target_include_directories(lvgl SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})

# App layer
file(GLOB_RECURSE APP_LAYER_SRCS CONFIGURE_DEPENDS
    app/*.c
    app/*.cc
    app/*.cpp
)
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/apps/app_template/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/apps/app_startup_anim/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/assets/images/(launcher_bg|sw_.*|arrow_state_on|internal_i2c_dev_chart|porta_i2c_.*|logo_.*|chg_arrow_.*)\\.c$")
set(APP_LAYER_INCS
    app/
)

# 桌面端源文件
file(GLOB_RECURSE APP_DESKTOP_BUILD_SRCS CONFIGURE_DEPENDS
    platforms/desktop/*.cpp
    platforms/desktop/*.cc
    platforms/desktop/*.c
)
add_executable(app_desktop_build ${APP_DESKTOP_BUILD_SRCS} ${APP_LAYER_SRCS})
target_include_directories(app_desktop_build PUBLIC ${APP_LAYER_INCS})
target_include_directories(app_desktop_build PRIVATE platforms/desktop)
target_link_libraries(app_desktop_build PUBLIC 
    mooncake 
    mooncake_log
    lvgl 
    ${SDL2_LIBRARIES}
    smooth_ui_toolkit
    pthread
)

file(GLOB_RECURSE APP_LAYER_SRCS
    ../../../app/*.c
    ../../../app/*.cc
    ../../../app/*.cpp
)
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/apps/app_template/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/apps/app_startup_anim/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*app/assets/images/(launcher_bg|sw_.*|arrow_state_on|internal_i2c_dev_chart|porta_i2c_.*|logo_.*|chg_arrow_.*)\\.c$")

set(APP_LAYER_INCS
    ../../../app
)

file(GLOB_RECURSE MY_HAL_SRCS
    ./hal/*.c
    ./hal/*.cc
    ./hal/*.cpp
)

idf_component_register(SRCS "app_main.cpp" ${APP_LAYER_SRCS} ${MY_HAL_SRCS}
                    INCLUDE_DIRS "." ${APP_LAYER_INCS}
                    EMBED_TXTFILES "../audio/canon_in_d.mp3" "../audio/startup_sfx.mp3" "../audio/shutdown_sfx.mp3")

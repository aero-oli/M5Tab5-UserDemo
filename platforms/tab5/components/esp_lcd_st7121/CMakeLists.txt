idf_component_register(SRCS "esp_lcd_st7121.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_lcd")

# This local driver declares its version in esp_lcd_st7121.h; no component manifest.

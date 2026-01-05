# components/status_led/CMakeLists.txt
idf_component_register(
    SRCS "src/status_led.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip
)
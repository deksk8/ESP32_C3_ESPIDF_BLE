# components/freertos_module/CMakeLists.txt
idf_component_register(
    SRCS "src/freertos_module.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
)
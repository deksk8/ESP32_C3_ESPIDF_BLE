cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32_C3_IDF)

# Adiciona diretórios de componentes customizados
set(EXTRA_COMPONENT_DIRS components)
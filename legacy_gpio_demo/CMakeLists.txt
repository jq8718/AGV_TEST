cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32p4" CACHE STRING "ESP-IDF target")
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(agv_scan_gpio_demo)

cmake_minimum_required(VERSION 3.20)
project(HardFaultPlugin LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "This plugin must be built with MSVC (Windows x64).")
endif()

add_library(HardFaultPlugin SHARED
    plugin/HardFaultPlugin.cpp
)

target_include_directories(HardFaultPlugin PRIVATE plugin)

target_compile_options(HardFaultPlugin PRIVATE /utf-8 /W3 /EHsc)
target_link_libraries(HardFaultPlugin PRIVATE pdh)

set_target_properties(HardFaultPlugin PROPERTIES
    OUTPUT_NAME "HardFaultPlugin_x64"
    PREFIX ""
)

# 静态链接 CRT，避免用户机器缺 VC 运行库导致 DLL 加载失败
set_property(TARGET HardFaultPlugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
set_property(TARGET HardFaultPlugin PROPERTY CXX_STANDARD 17)
set_property(TARGET HardFaultPlugin PROPERTY CXX_STANDARD_REQUIRED ON)

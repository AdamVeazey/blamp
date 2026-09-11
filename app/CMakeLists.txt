cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blamp)

target_include_directories(app PRIVATE inc)

target_sources(app PRIVATE
    src/Blamp_Audio.cpp
    src/Blamp_Blampknob.cpp
    src/Blamp.cpp
    src/CLI_blampknob.cpp
    src/DisplayUI.cpp
    src/main.cpp
)
cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blamp)

include(FetchContent)
FetchContent_Declare(
  EDF
  GIT_REPOSITORY https://github.com/AdamVeazey/edf.git
  GIT_TAG        dev   # Or a specific commit hash/tag/branch
)

FetchContent_MakeAvailable(EDF)

# FIX: Force EDF to inherit Zephyr's compiler options, include paths, and Picolibc specs
target_link_libraries(EDF PRIVATE zephyr_interface)

target_include_directories(app PRIVATE inc)

target_sources(app PRIVATE
    src/Blamp_Audio.cpp
    src/Blamp_Blampknob.cpp
    src/Blamp.cpp
    src/CLI_blampknob.cpp
    src/DisplayUI.cpp
    src/FSC_BT1026.cpp
    src/main.cpp
)

target_link_libraries(app PRIVATE EDF)
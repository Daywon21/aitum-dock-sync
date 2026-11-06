cmake_minimum_required(VERSION 3.28...3.30)

project(aitum-dock-sync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(CMC_ROOT "${CMAKE_SOURCE_DIR}/../../captain-multi-chat")
set(OBS_LIBOBS_DIR "${CMC_ROOT}/.deps/obs-studio-32.1.2/libobs")
set(OBS_FRONTEND_DIR "${CMC_ROOT}/.deps/obs-studio-32.1.2/frontend/api")
set(OBS_CONFIG_DIR "${CMC_ROOT}/.deps/obs-dev")
set(OBS_LIB_DIR "${OBS_CONFIG_DIR}/lib")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(aitum-dock-sync MODULE
  src/plugin-main.cpp
  src/dock-control.cpp
  src/picks-store.cpp
  src/platform-dock.cpp
  src/vertical-control.cpp
  src/style-dump.cpp
)

target_include_directories(aitum-dock-sync PRIVATE
  "${OBS_LIBOBS_DIR}"
  "${OBS_FRONTEND_DIR}"
  "${OBS_CONFIG_DIR}"
  src
)

target_link_directories(aitum-dock-sync PRIVATE
  "${OBS_LIB_DIR}"
)

target_link_libraries(aitum-dock-sync PRIVATE
  "${OBS_LIB_DIR}/obs.lib"
  "${OBS_LIB_DIR}/obs-frontend-api.lib"
  Qt6::Core
  Qt6::Widgets
)

if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(aitum-dock-sync PRIVATE /W3 /permissive- /Zc:__cplusplus /FIintrin.h)
  target_link_options(aitum-dock-sync PRIVATE /INCREMENTAL:NO)
  target_compile_definitions(aitum-dock-sync PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
endif()

set_target_properties(aitum-dock-sync PROPERTIES
  PREFIX ""
  OUTPUT_NAME "aitum-dock-sync"
)

option(ADS_BUILD_TESTS "Build Aitum Dock Sync unit tests (QtTest + ctest)" OFF)
if(ADS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

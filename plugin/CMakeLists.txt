cmake_minimum_required(VERSION 3.28...3.30)

project(aitum-dock-sync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

if(NOT DEFINED OBS_LIBOBS_DIR)
  if(DEFINED ENV{OBS_LIBOBS_DIR})
    set(OBS_LIBOBS_DIR "$ENV{OBS_LIBOBS_DIR}")
  else()
    set(OBS_LIBOBS_DIR "${CMAKE_SOURCE_DIR}/../.deps/obs-studio/libobs")
  endif()
endif()

if(NOT DEFINED OBS_FRONTEND_DIR)
  if(DEFINED ENV{OBS_FRONTEND_DIR})
    set(OBS_FRONTEND_DIR "$ENV{OBS_FRONTEND_DIR}")
  else()
    set(OBS_FRONTEND_DIR "${CMAKE_SOURCE_DIR}/../.deps/obs-studio/frontend/api")
  endif()
endif()

if(NOT DEFINED OBS_LIB_DIR)
  if(DEFINED ENV{OBS_LIB_DIR})
    set(OBS_LIB_DIR "$ENV{OBS_LIB_DIR}")
  else()
    set(OBS_LIB_DIR "${CMAKE_SOURCE_DIR}/../.deps/obs-dev/lib")
  endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(aitum-dock-sync MODULE
  src/plugin-main.cpp
  src/dock-control.cpp
)

target_include_directories(aitum-dock-sync PRIVATE
  "${OBS_LIBOBS_DIR}"
  "${OBS_FRONTEND_DIR}"
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

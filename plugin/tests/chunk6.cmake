# Plugin sources under test (globbed so later chunks' new source files are included) and the
# OBS-boundary fakes. Later chunks' test executables link these two object libraries too.
file(GLOB ADS_PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp")
add_library(ads_plugin_under_test OBJECT ${ADS_PLUGIN_SOURCES})
add_library(ads_fakes OBJECT
  ${CMAKE_CURRENT_LIST_DIR}/fakes/fake_obs.hpp
  ${CMAKE_CURRENT_LIST_DIR}/fakes/fake_obs.cpp
)
foreach(lib ads_plugin_under_test ads_fakes)
  target_include_directories(${lib} PUBLIC
    "${OBS_LIBOBS_DIR}" "${OBS_FRONTEND_DIR}" "${OBS_CONFIG_DIR}"
    ${CMAKE_CURRENT_LIST_DIR}/../src ${CMAKE_CURRENT_LIST_DIR}/fakes)
  target_link_libraries(${lib} PUBLIC Qt6::Core Qt6::Widgets)
  if(MSVC)
    target_compile_options(${lib} PUBLIC /W3 /permissive- /Zc:__cplusplus /FIintrin.h)
    target_compile_definitions(${lib} PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
endforeach()

add_executable(test_plugin_stream ${CMAKE_CURRENT_LIST_DIR}/test_plugin_stream.cpp)
target_link_libraries(test_plugin_stream PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_stream_start_clicks_only_ticked_outputs test_stream_stop_stops_all_outputs)
  add_test(NAME ${t} COMMAND test_plugin_stream ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

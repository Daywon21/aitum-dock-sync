# Chunk 11: dock restyle + live indicator.
add_executable(test_dock_style ${CMAKE_CURRENT_LIST_DIR}/test_dock_style.cpp)
target_link_libraries(test_dock_style PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_live_indicator_follows_output_state test_dock_uses_obs_theme_classes_and_margins)
  add_test(NAME ${t} COMMAND test_dock_style ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

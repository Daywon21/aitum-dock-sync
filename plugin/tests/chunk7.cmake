add_executable(test_platform_dock ${CMAKE_CURRENT_LIST_DIR}/test_platform_dock.cpp)
target_link_libraries(test_platform_dock PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_dock_lists_aitum_outputs_for_current_profile test_tick_while_live_starts_output
          test_untick_while_live_stops_output test_toggle_while_offline_only_saves
          test_profile_change_reloads_list_and_picks)
  add_test(NAME ${t} COMMAND test_platform_dock ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

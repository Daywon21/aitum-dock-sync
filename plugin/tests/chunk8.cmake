add_executable(test_live_refresh ${CMAKE_CURRENT_LIST_DIR}/test_live_refresh.cpp)
target_link_libraries(test_live_refresh PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_config_change_adds_new_output_unticked test_new_aitum_groupbox_adds_output_unticked
          test_removed_output_leaves_dock_keeps_pick)
  add_test(NAME ${t} COMMAND test_live_refresh ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

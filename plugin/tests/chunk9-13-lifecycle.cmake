# Chunk 13: Vertical outputs followed through libobs signals (Aitum Vertical auto-start, connecting state).
add_executable(test_vertical_lifecycle ${CMAKE_CURRENT_LIST_DIR}/test_vertical_lifecycle.cpp)
target_link_libraries(test_vertical_lifecycle PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_unticked_vertical_auto_started_is_stopped_once_live
          test_ticked_vertical_already_connecting_is_not_started_again
          test_ticked_vertical_started_when_aitum_does_not_auto_start
          test_untick_while_connecting_stops_once_live
          test_stop_pass_waits_for_connecting_vertical)
  add_test(NAME ${t} COMMAND test_vertical_lifecycle ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

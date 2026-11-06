# Chunk 14: hand-started Vertical outputs live at main stream start are left running and ticked.
add_executable(test_hand_started_vertical ${CMAKE_CURRENT_LIST_DIR}/test_hand_started_vertical.cpp)
target_link_libraries(test_hand_started_vertical PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)
add_test(NAME test_hand_started_vertical_left_running_and_ticked_at_start
         COMMAND test_hand_started_vertical test_hand_started_vertical_left_running_and_ticked_at_start)
set_tests_properties(test_hand_started_vertical_left_running_and_ticked_at_start PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

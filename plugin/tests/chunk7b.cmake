# Added test (Captain Dark 2026-11-02): "Aitum Multistream not found".
add_executable(test_dock_not_found ${CMAKE_CURRENT_LIST_DIR}/test_dock_not_found.cpp)
target_link_libraries(test_dock_not_found PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)
add_test(NAME test_dock_shows_not_found_without_aitum COMMAND test_dock_not_found test_dock_shows_not_found_without_aitum)
set_tests_properties(test_dock_shows_not_found_without_aitum PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Preview tool for Captain Dark (not a test, not shipped): renders the dock to PNG files.
add_executable(dock_preview ${CMAKE_CURRENT_LIST_DIR}/preview/dock_preview.cpp)
target_link_libraries(dock_preview PRIVATE ads_plugin_under_test ads_fakes Qt6::Widgets)

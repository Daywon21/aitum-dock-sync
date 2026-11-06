# Chunk 10: Aitum Vertical outputs. The new OBS-boundary fakes (proc handler, calldata,
# obs_output, obs_data, Aitum Vertical procs, obs-websocket + aitum-multistream vendor) join
# ads_fakes, because the plugin now calls those libobs functions and every test exe links it.
target_sources(ads_fakes PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/fakes/fake_obs_api.hpp
  ${CMAKE_CURRENT_LIST_DIR}/fakes/fake_obs_api.cpp
)

add_executable(test_vertical_outputs ${CMAKE_CURRENT_LIST_DIR}/test_vertical_outputs.cpp)
target_link_libraries(test_vertical_outputs PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_dock_groups_main_and_vertical_outputs test_stream_start_starts_ticked_vertical_outputs
          test_stream_stop_stops_all_vertical_outputs test_vertical_tick_while_live_starts_untick_stops
          test_vertical_toggle_while_offline_only_saves test_new_vertical_output_appears_unticked
          test_stream_key_never_read_or_logged test_main_only_without_vertical_api)
  add_test(NAME ${t} COMMAND test_vertical_outputs ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

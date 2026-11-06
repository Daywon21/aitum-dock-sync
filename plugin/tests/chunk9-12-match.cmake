# Chunk 12: rows match the Audio Mixer font (theme class text-small) + one-time style dump.
add_executable(test_style_match ${CMAKE_CURRENT_LIST_DIR}/test_style_match.cpp)
target_link_libraries(test_style_match PRIVATE ads_plugin_under_test ads_fakes Qt6::Test)

foreach(t test_rows_match_audio_mixer_font_under_yami_rules test_style_dump_logged_once)
  add_test(NAME ${t} COMMAND test_style_match ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

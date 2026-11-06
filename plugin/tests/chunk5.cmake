add_executable(test_picks_store
  ${CMAKE_CURRENT_LIST_DIR}/test_picks_store.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/picks-store.cpp
)
target_include_directories(test_picks_store PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src)
target_link_libraries(test_picks_store PRIVATE Qt6::Core Qt6::Test)

foreach(t test_picks_saved_per_profile test_picks_survive_reload test_unknown_output_defaults_unticked)
  add_test(NAME ${t} COMMAND test_picks_store ${t})
endforeach()

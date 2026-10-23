sandreco_add_test_process(sand_test_truth_filler)

target_sources(sand_test_truth_filler PRIVATE test_truth_filler.cpp)

target_include_directories(sand_test_truth_filler PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

target_link_libraries(sand_test_truth_filler PRIVATE sand_common_truth_filler sand_edep_reader duneanaobj::StandardRecord)

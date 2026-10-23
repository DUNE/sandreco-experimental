sandreco_add_test_process(sand_test_fast_reco)

target_sources(sand_test_fast_reco PRIVATE test_fast_reco.cpp)

target_include_directories(sand_test_fast_reco PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

target_link_libraries(sand_test_fast_reco PRIVATE sand_common_truth_filler sand_edep_reader duneanaobj::StandardRecord)

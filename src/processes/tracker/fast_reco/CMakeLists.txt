sandreco_add_process(sand_tracker_fast_reco)

target_sources(sand_tracker_fast_reco PRIVATE fast_reco.cpp common_utils.cpp)

target_include_directories(sand_tracker_fast_reco PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

add_dependencies(sand_tracker_fast_reco sand_edep_reader)

target_link_libraries(sand_tracker_fast_reco PRIVATE sand_edep_reader sand_spill_maps sand_common_truth_filler ROOT::MathCore ROOT::EG duneanaobj::StandardRecord)

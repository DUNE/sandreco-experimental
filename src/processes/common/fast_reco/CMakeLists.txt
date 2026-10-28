sandreco_add_process(sand_common_fast_reco)

target_sources(sand_common_fast_reco
  PRIVATE
  fast_reco.cpp
  fast_reco_details.cpp
)

target_include_directories(sand_common_fast_reco PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

add_dependencies(sand_common_fast_reco sand_edep_reader)

target_link_libraries(sand_common_fast_reco PRIVATE sand_edep_reader sand_common_truth_filler ROOT::MathCore ROOT::EG duneanaobj::StandardRecord)

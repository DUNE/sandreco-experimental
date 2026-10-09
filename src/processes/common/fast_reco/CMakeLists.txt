sandreco_add_process(sand_common_fast_reco)

target_sources(sand_common_fast_reco
  PRIVATE
  fast_reco.cpp
  fast_reco_details.cpp
)

add_dependencies(sand_common_fast_reco sand_edep_reader_refactor)

target_link_libraries(sand_common_fast_reco PRIVATE sand_edep_reader_refactor ROOT::MathCore ROOT::EG duneanaobj::StandardRecord)

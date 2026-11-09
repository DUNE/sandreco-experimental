sandreco_add_process(sand_fast_grain_reco)

target_sources(sand_fast_grain_reco
  PRIVATE
  reco.cpp
)

target_link_libraries(sand_fast_grain_reco PRIVATE sand_edep_reader sand_spill_maps ROOT::MathCore ROOT::EG duneanaobj::StandardRecord)

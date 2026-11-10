sandreco_add_process(sand_fast_ecal_reco)

target_sources(sand_fast_ecal_reco
  PRIVATE
  reco.cpp
)

target_link_libraries(sand_fast_ecal_reco PRIVATE sand_edep_reader sand_spill_maps sand_geoinfo ROOT::MathCore duneanaobj::StandardRecord)

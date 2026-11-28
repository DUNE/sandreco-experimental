sandreco_add_process(sand_fast_tracker_reco)

target_sources(sand_fast_tracker_reco
  PRIVATE
  reco.cpp
)

target_link_libraries(sand_fast_tracker_reco PRIVATE sand_edep_reader sand_spill_maps sand_particle_table ROOT::MathCore duneanaobj::StandardRecord)

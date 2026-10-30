sandreco_add_process(sand_tracker_fast_reco)

target_sources(sand_tracker_fast_reco PRIVATE fast_reco.cpp common_utils.cpp)

target_link_libraries(sand_tracker_fast_reco PRIVATE sand_edep_reader sand_spill_maps ROOT::MathCore ROOT::EG duneanaobj::StandardRecord)

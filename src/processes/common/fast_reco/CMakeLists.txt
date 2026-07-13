sandreco_add_process(sand_common_fast_reco)

target_sources(sand_common_fast_reco
  PRIVATE
  fast_reco.cpp
)

# add_dependencies(sand_common_fast_reco sand_edep_reader sand_genie_reader)
# target_link_libraries(sand_common_fast_reco PRIVATE sand_edep_reader sand_genie_reader ROOT::RIO ROOT::Tree ROOT::EG)

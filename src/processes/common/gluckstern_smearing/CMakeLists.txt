sandreco_add_process(sand_common_gluckstern_smearing)

target_sources(sand_common_gluckstern_smearing
  PRIVATE
  gluckstern_smearing.cpp
)

target_link_libraries(sand_common_gluckstern_smearing
  PRIVATE
  sand_root_tgeomanager
  sand_edep_reader
  sand_spill_maps
  sand_particle_table
  ROOT::MathCore
  duneanaobj::StandardRecord
)

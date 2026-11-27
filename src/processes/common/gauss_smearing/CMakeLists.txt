sandreco_add_process(sand_common_gauss_smearing)

target_sources(sand_common_gauss_smearing
  PRIVATE
  gauss_smearing.cpp
)

target_link_libraries(sand_common_gauss_smearing PRIVATE sand_particle_table ROOT::MathCore duneanaobj::StandardRecord)

sandreco_add_process(sand_fast_neutrino)

target_sources(sand_fast_neutrino
  PRIVATE
  neutrino.cpp
)

target_link_libraries(sand_fast_neutrino PRIVATE duneanaobj::StandardRecord)

sandreco_add_complex_data(sand_spill_maps EXPORT_HEADERS id_range.hpp g4_id_map.hpp spill_maps.hpp)
target_sources(sand_spill_maps PRIVATE g4_id_map.cpp spill_maps.cpp)
target_link_libraries(sand_spill_maps PUBLIC sand_edep_reader PRIVATE ROOT::Physics)

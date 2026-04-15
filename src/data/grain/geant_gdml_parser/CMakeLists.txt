set(HDRS geant_gdml_parser.hpp)

sandreco_add_complex_data(sand_grain_geant_gdml_parser EXPORT_HEADERS ${HDRS})

find_package(Geant4 REQUIRED)

target_sources(sand_grain_geant_gdml_parser PRIVATE geant_gdml_parser.cpp)

target_link_libraries(sand_grain_geant_gdml_parser PUBLIC ${Geant4_LIBRARIES} PRIVATE z)

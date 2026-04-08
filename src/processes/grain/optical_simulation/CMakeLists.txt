sandreco_add_process(sand_grain_optical_simulation)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

target_sources(sand_grain_optical_simulation PRIVATE
    optical_simulation.cpp
    ${HEADERS}
    ${SOURCES})

target_include_directories(sand_grain_optical_simulation PRIVATE "${CMAKE_SOURCE_DIR}/src/data/grain")

target_link_libraries(sand_grain_optical_simulation
                      PRIVATE ROOT::RIO ROOT::Tree ${Geant4_LIBRARIES} z
                      sand_grain_geant_run_manager sand_grain_geant_gdml_parser
                      sand_root_tgeomanager sand_geoinfo sand_edep_reader)

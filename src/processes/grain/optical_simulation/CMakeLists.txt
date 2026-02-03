add_library(sand_grain_optical_simulation)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

target_sources(sand_grain_optical_simulation PRIVATE
    optical_simulation.cpp
    ${HEADERS}
    ${SOURCES})

target_include_directories(sand_grain_optical_simulation PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(sand_grain_optical_simulation
                      PUBLIC ufw::ufw
                      PRIVATE ROOT::RIO ROOT::Tree ${Geant4_LIBRARIES} z
                      sand_grain_geant_run_manager sand_grain_geant_gdml_parser
                      sand_root_tgeomanager sand_geoinfo sand_edep_reader)

install(TARGETS sand_grain_optical_simulation EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(optical_simulation)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

find_package(Geant4 REQUIRED)

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(optical_simulation PRIVATE
    optical_simulation.cpp
    ${HEADERS}
    ${SOURCES})

target_include_directories(optical_simulation PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(optical_simulation ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z EDepSim::edepsim_io geant_run_manager geant_gdml_parser root_tgeomanager geoinfo edep_reader)

install(TARGETS optical_simulation EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

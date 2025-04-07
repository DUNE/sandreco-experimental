add_library(G4_optmen_edepsim)

include_directories(../../../include)
include_directories(../../common)
include_directories(../../data)
include_directories(.)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

find_package(Geant4 REQUIRED)
include_directories(${Geant4_USE_FILE})

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(G4_optmen_edepsim PRIVATE G4_optmen_edepsim.cpp ${SOURCES})

target_link_libraries(G4_optmen_edepsim ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} EDepSim::edepsim_io G4_optmen_runmanager)

install(TARGETS G4_optmen_edepsim EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

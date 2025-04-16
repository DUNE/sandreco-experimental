add_library(optical_simulation)

include_directories(../../../../include)
include_directories(../../../common)
include_directories(../../../data)
include_directories(.)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

find_package(Geant4 REQUIRED)

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(optical_simulation PRIVATE optical_simulation.cpp ${SOURCES})

target_link_libraries(optical_simulation ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z EDepSim::edepsim_io G4_optmen_runmanager EdepReader)

install(TARGETS optical_simulation EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

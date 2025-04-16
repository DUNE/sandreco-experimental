add_library(detector_response)

include_directories(../../../../include)
include_directories(../../../data)
include_directories(.)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

find_package(Geant4 REQUIRED)

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(detector_response PRIVATE detector_response.cpp ${SOURCES})

target_link_libraries(detector_response ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z EDepSim::edepsim_io geant_run_manager edep_reader)

install(TARGETS detector_response EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

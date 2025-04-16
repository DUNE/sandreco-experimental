add_library(detector_response)

find_package(Geant4 REQUIRED)

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(detector_response PRIVATE detector_response.cpp)

target_include_directories(detector_response PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(detector_response ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z EDepSim::edepsim_io geant_run_manager edep_reader)

install(TARGETS detector_response EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

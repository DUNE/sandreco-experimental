add_library(geant_run_manager)

include_directories(../../../../include)
include_directories(../../../common)
include_directories(.)

find_package(Geant4 REQUIRED)

target_sources(geant_run_manager PRIVATE geant_run_manager.cpp)

target_link_libraries(geant_run_manager ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z)

install(TARGETS geant_run_manager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

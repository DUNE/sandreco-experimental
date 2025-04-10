add_library(G4_optmen_runmanager)

include_directories(../../../include)
include_directories(../../common)
include_directories(.)

find_package(Geant4 REQUIRED)

include_directories(${Geant4_USE_FILE})

target_sources(G4_optmen_runmanager PRIVATE G4_optmen_runmanager.cpp)

target_link_libraries(G4_optmen_runmanager ROOT::RIO ROOT::Tree ufw::ufw ${Geant4_LIBRARIES} z)

install(TARGETS G4_optmen_runmanager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

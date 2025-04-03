add_library(Geant4_example)

include_directories(../../../include)
include_directories(../../common)

find_package(Geant4 REQUIRED)
include_directories(${Geant4_USE_FILE})

target_sources(Geant4_example PRIVATE Geant4_example.cpp)

target_link_libraries(Geant4_example ROOT::RIO ROOT::Tree ufw::ufw TTreeStreamer ${Geant4_LIBRARIES})

install(TARGETS Geant4_example EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

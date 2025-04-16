add_library(edep_reader)

include_directories(../../../../include)
include_directories(../../../common)
include_directories(.)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)


target_sources(edep_reader PRIVATE edep_reader.cpp EDEPTree.cpp EDEPTrajectory.cpp EDEPHit.cpp)

target_link_libraries(edep_reader ROOT::RIO ROOT::Tree ufw::ufw EDepSim::edepsim_io)

install(TARGETS edep_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

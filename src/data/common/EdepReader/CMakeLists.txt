add_library(EdepReader)

include_directories(../../../../include)
include_directories(../../../common)
include_directories(.)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)


target_sources(EdepReader PRIVATE EdepReader.cpp EDEPTree.cpp EDEPTrajectory.cpp EDEPHit.cpp)

target_link_libraries(EdepReader ROOT::RIO ROOT::Tree ufw::ufw EDepSim::edepsim_io)

install(TARGETS EdepReader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(edep_reader)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(edep_reader PRIVATE
    edep_reader.cpp
    EDEPTree.h
    EDEPTree.cpp
    EDEPTrajectoryPoint.h
    EDEPTrajectory.h
    EDEPTrajectory.cpp
    EDEPHit.h
    EDEPHit.cpp
    EDEPUtils.h)

target_include_directories(edep_reader PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(edep_reader ROOT::RIO ROOT::Tree ufw::ufw EDepSim::edepsim_io)

install(TARGETS edep_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

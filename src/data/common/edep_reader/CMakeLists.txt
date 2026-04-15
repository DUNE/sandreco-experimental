set(HDRS
    EDEPTree.h
    EDEPTrajectoryPoint.h
    EDEPTrajectory.h
    EDEPHit.h
    EDEPUtils.h
    edep_reader.hpp)

sandreco_add_complex_data(sand_edep_reader EXPORT_HEADERS ${HDRS})

find_package(EDepSim NAMES EDepSim REQUIRED)
#find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(sand_edep_reader PRIVATE
    edep_reader.cpp
    EDEPTree.cpp
    EDEPTrajectory.cpp
    EDEPHit.cpp)

target_link_libraries(sand_edep_reader PUBLIC EDepSim::edepsim_io PRIVATE ROOT::RIO ROOT::Tree sand_root_tgeomanager)

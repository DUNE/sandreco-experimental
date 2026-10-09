set(HDRS
    EDEPTree.h
    EDEPTrajectoryPoint.h
    EDEPTrajectory.h
    EDEPHit.h
    edep_reader_refactor.hpp)

sandreco_add_complex_data(sand_edep_reader_refactor EXPORT_HEADERS ${HDRS})

find_package(EDepSim NAMES EDepSim REQUIRED)

target_sources(sand_edep_reader_refactor PRIVATE
    edep_reader_refactor.cpp
    EDEPTree.cpp
    EDEPTrajectory.cpp)

target_link_libraries(sand_edep_reader_refactor PUBLIC EDepSim::edepsim_io PRIVATE ROOT::RIO ROOT::Tree sand_root_tgeomanager)

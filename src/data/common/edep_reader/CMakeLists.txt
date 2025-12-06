add_library(edep_reader)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

set(HDRS EDEPTree.h
    EDEPTrajectoryPoint.h
    EDEPTrajectory.h
    EDEPHit.h
    EDEPUtils.h
    edep_reader.hpp)

target_sources(edep_reader PRIVATE
    ${HDRS}
    edep_reader.cpp
    EDEPTree.cpp
    EDEPTrajectory.cpp
    EDEPHit.cpp)

target_include_directories(edep_reader PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(edep_reader ROOT::RIO ROOT::Tree ufw::ufw EDepSim::edepsim_io)

install(TARGETS edep_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(edep_reader INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

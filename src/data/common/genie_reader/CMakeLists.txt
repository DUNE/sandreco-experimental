add_library(genie_reader)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(genie_reader PRIVATE
    genie_reader.cpp
    GenieWrapper.cpp)

target_include_directories(genie_reader PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(genie_reader ROOT::RIO ROOT::Tree ufw::ufw EDepSim::edepsim_io)

install(TARGETS genie_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

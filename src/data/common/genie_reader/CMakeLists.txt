add_library(sand_genie_reader)

find_package(EDepSim NAMES EDepSim REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(sand_genie_reader PRIVATE
    genie_reader.cpp
    GenieWrapper.cpp)

target_include_directories(sand_genie_reader PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(sand_genie_reader PUBLIC ufw::ufw PRIVATE ROOT::RIO ROOT::Tree EDepSim::edepsim_io)

install(TARGETS sand_genie_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

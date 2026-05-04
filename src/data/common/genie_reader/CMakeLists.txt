sandreco_add_complex_data(sand_genie_reader)

find_package(EDepSim NAMES EDepSim REQUIRED)
#find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(sand_genie_reader PRIVATE
    genie_reader.cpp
    GenieWrapper.cpp)

target_link_libraries(sand_genie_reader PRIVATE ROOT::RIO ROOT::Tree EDepSim::edepsim_io)

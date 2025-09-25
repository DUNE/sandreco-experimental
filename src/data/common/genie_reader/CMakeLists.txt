add_library(genie_reader)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(genie_reader PRIVATE
    genie_reader.cpp
    GenieWrapper.h)

target_include_directories(genie_reader PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(genie_reader ROOT::RIO ROOT::Tree ufw::ufw)

install(TARGETS genie_reader EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(geomanager)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(geomanager PRIVATE geomanager.hpp geomanager.cpp)

target_include_directories(geomanager PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(geomanager ROOT::RIO ROOT::Tree ROOT::Geom ROOT::MathCore ROOT::Matrix ufw::ufw)

install(TARGETS geomanager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

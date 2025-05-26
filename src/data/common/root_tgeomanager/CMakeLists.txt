add_library(root_tgeomanager)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(root_tgeomanager PRIVATE
               root_tgeomanager.hpp
               root_tgeomanager.cpp)

target_include_directories(root_tgeomanager PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(root_tgeomanager ROOT::RIO ROOT::Tree ROOT::Geom ROOT::MathCore ROOT::Matrix ufw::ufw)

install(TARGETS root_tgeomanager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

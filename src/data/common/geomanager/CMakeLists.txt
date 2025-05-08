add_library(geomanager)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(geomanager PRIVATE
               drift_manager.hpp
               drift_manager.cpp
               ecal_manager.hpp
               ecal_manager.cpp
               grain_manager.hpp
               grain_manager.cpp
               stt_manager.hpp
               stt_manager.cpp
               tracker_manager.hpp
               tracker_manager.cpp
               subdetector_manager.hpp
               subdetector_manager.cpp
               geomanager.hpp
               geomanager.cpp)

target_include_directories(geomanager PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(geomanager ROOT::RIO ROOT::Tree ROOT::Geom ROOT::MathCore ROOT::Matrix ufw::ufw)

install(TARGETS geomanager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

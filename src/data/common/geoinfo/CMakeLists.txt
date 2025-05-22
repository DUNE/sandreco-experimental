add_library(geoinfo)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(geoinfo PRIVATE
               drift_info.hpp
               drift_info.cpp
               ecal_info.hpp
               ecal_info.cpp
               grain_info.hpp
               grain_info.cpp
               stt_info.hpp
               stt_info.cpp
               tracker_info.hpp
               tracker_info.cpp
               subdetector_info.hpp
               subdetector_info.cpp
               geoinfo.hpp
               geoinfo.cpp)

target_include_directories(geoinfo PRIVATE . .. ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(geoinfo ROOT::Geom ROOT::MathCore ROOT::Matrix geant_gdml_parser ufw::ufw)

install(TARGETS geoinfo EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

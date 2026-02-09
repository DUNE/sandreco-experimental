add_library(sand_geoinfo)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

set(HDRS drift_info.hpp 
         ecal_info.hpp
         grain_info.hpp
         stt_info.hpp
         tracker_info.hpp
         subdetector_info.hpp
         geoinfo.hpp
         BVH.hpp
         BVH_analyzer.hpp)

target_sources(sand_geoinfo PRIVATE
               ${HDRS}
               drift_info.cpp
               ecal_info.cpp
               grain_info.cpp
               stt_info.cpp
               tracker_info.cpp
               subdetector_info.cpp
               geoinfo.cpp)

target_include_directories(sand_geoinfo PRIVATE . .. ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(sand_geoinfo PUBLIC ROOT::Geom ROOT::MathCore ROOT::Matrix ufw::ufw PRIVATE sand_grain_geant_gdml_parser sand_root_tgeomanager)

install(TARGETS sand_geoinfo EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(sand_geoinfo INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

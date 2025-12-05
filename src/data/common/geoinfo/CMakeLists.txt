add_library(geoinfo)

find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

set(HDRS drift_info.hpp 
         ecal_info.hpp
         grain_info.hpp
         stt_info.hpp
         tracker_info.hpp
         subdetector_info.hpp
         geoinfo.hpp)

target_sources(geoinfo PRIVATE
               ${HDRS}
               drift_info.cpp
               ecal_info.cpp
               grain_info.cpp
               stt_info.cpp
               tracker_info.cpp
               subdetector_info.cpp
               geoinfo.cpp)

target_include_directories(geoinfo PRIVATE . .. ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(geoinfo ROOT::Geom ROOT::MathCore ROOT::Matrix geant_gdml_parser root_tgeomanager ufw::ufw)

install(TARGETS geoinfo EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(geoinfo INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private>)
endif()

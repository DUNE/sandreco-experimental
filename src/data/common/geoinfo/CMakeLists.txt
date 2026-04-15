set(HDRS drift_info.hpp 
         ecal_info.hpp
         grain_info.hpp
         stt_info.hpp
         tracker_info.hpp
         subdetector_info.hpp
         geoinfo.hpp)

sandreco_add_complex_data(sand_geoinfo EXPORT_HEADERS ${HDRS})

#find_package(ROOT REQUIRED COMPONENTS Geom Physics Matrix MathCore RIO Tree)

target_sources(sand_geoinfo PRIVATE
               drift_info.cpp
               ecal_info.cpp
               grain_info.cpp
               stt_info.cpp
               tracker_info.cpp
               subdetector_info.cpp
               geoinfo.cpp
               BVH.hpp
               BVH.cpp)

target_include_directories(sand_geoinfo PRIVATE ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(sand_geoinfo PUBLIC ROOT::GenVector ROOT::Geom ROOT::MathCore ROOT::Matrix
                      PRIVATE sand_grain_geant_gdml_parser sand_root_tgeomanager)

add_library(sand_tracker_clustering)

target_sources(sand_tracker_clustering PRIVATE clustering.cpp)

target_include_directories(sand_tracker_clustering PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_tracker_clustering PUBLIC ufw::ufw PRIVATE sand_edep_reader sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_tracker_clustering EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
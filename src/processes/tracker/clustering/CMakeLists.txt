add_library(clustering)

target_sources(clustering PRIVATE clustering.cpp)

target_include_directories(clustering PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(clustering ufw::ufw sand_edep_reader sand_root_tgeomanager sand_geoinfo)


install(TARGETS clustering EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

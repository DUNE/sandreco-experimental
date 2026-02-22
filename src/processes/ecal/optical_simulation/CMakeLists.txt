add_library(sand_ecal_optical_simulation)

target_sources(sand_ecal_optical_simulation PRIVATE optical_simulation.cpp)

target_include_directories(sand_ecal_optical_simulation PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_ecal_optical_simulation PUBLIC ufw::ufw PRIVATE sand_edep_reader sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_ecal_optical_simulation EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

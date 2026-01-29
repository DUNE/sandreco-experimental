add_library(fast_digi_drift)

target_sources(fast_digi_drift PRIVATE fast_digi_drift.cpp)

target_include_directories(fast_digi_drift PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(fast_digi_drift ufw::ufw sand_edep_reader sand_root_tgeomanager sand_geoinfo)


install(TARGETS fast_digi_drift EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

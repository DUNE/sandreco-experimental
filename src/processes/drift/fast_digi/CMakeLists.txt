add_library(sand_drift_drift_fast_digi)

target_sources(sand_drift_drift_fast_digi PRIVATE drift_fast_digi.cpp)

target_include_directories(sand_drift_drift_fast_digi PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_drift_drift_fast_digi PUBLIC ufw::ufw  PRIVATE sand_edep_reader sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_drift_drift_fast_digi EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

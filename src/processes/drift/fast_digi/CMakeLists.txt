add_library(drift_fast_digi)

target_sources(drift_fast_digi PRIVATE drift_fast_digi.cpp)

target_include_directories(drift_fast_digi PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(drift_fast_digi ufw::ufw edep_reader root_tgeomanager geoinfo)


install(TARGETS drift_fast_digi EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(fast_digi)

target_sources(fast_digi PRIVATE fast_digi.cpp)

target_include_directories(fast_digi PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(fast_digi ufw::ufw sand_edep_reader root_tgeomanager sand_geoinfo)


install(TARGETS fast_digi EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

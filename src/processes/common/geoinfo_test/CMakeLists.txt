add_library(sand_common_geoinfo_test)

target_sources(sand_common_geoinfo_test PRIVATE geoinfo_test.cpp ${SOURCES})

target_include_directories(sand_common_geoinfo_test PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_common_geoinfo_test PRIVATE ROOT::Core ufw::ufw sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_common_geoinfo_test EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

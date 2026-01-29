add_library(geoinfo_test)

target_sources(geoinfo_test PRIVATE geoinfo_test.cpp ${SOURCES})

target_include_directories(geoinfo_test PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(geoinfo_test ROOT::Core ufw::ufw root_tgeomanager sand_geoinfo)

install(TARGETS geoinfo_test EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(sand_grain_volumereco)

target_sources(sand_grain_volumereco PRIVATE volumereco.cpp ${SOURCES})

target_include_directories(sand_grain_volumereco PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_grain_volumereco PUBLIC ufw::ufw sand_cl PRIVATE sand_geoinfo sand_hdf5)

install(TARGETS sand_grain_volumereco EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

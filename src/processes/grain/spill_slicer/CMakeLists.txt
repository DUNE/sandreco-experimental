add_library(sand_grain_spill_slicer)

target_sources(sand_grain_spill_slicer PRIVATE spill_slicer.cpp ${SOURCES})

target_include_directories(sand_grain_spill_slicer PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(sand_grain_spill_slicer ROOT::Core ufw::ufw)

install(TARGETS sand_grain_spill_slicer EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(spill_slicer)

target_sources(spill_slicer PRIVATE spill_slicer.cpp ${SOURCES})

target_include_directories(spill_slicer PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(spill_slicer ROOT::Core ufw::ufw)

install(TARGETS spill_slicer EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

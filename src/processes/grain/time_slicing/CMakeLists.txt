add_library(time_slicing)

target_sources(time_slicing PRIVATE time_slicing.cpp ${SOURCES})

target_include_directories(time_slicing PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(time_slicing ufw::ufw)

install(TARGETS time_slicing EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

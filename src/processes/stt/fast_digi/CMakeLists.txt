add_library(stt_fast_digi)

target_sources(stt_fast_digi PRIVATE stt_fast_digi.cpp)

target_include_directories(stt_fast_digi PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(stt_fast_digi ufw::ufw edep_reader root_tgeomanager geoinfo)


install(TARGETS stt_fast_digi EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

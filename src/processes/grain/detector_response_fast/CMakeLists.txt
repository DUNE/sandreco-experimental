add_library(sand_grain_detector_response_fast)

target_sources(sand_grain_detector_response_fast PRIVATE detector_response_fast.cpp)

target_include_directories(sand_grain_detector_response_fast PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_grain_detector_response_fast PUBLIC ufw::ufw PRIVATE ROOT::Core sand_edep_reader sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_grain_detector_response_fast EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

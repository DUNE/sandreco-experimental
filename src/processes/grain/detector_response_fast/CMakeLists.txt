add_library(detector_response_fast)

target_sources(detector_response_fast PRIVATE detector_response_fast.cpp)

target_include_directories(detector_response_fast PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(detector_response_fast ufw::ufw edep_reader)

install(TARGETS detector_response_fast EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

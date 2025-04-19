add_library(detector_response)

target_sources(detector_response PRIVATE detector_response.cpp)

target_include_directories(detector_response PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(detector_response ufw::ufw edep_reader)

install(TARGETS detector_response EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

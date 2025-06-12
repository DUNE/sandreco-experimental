add_library(tracklet_finder)

target_sources(tracklet_finder PRIVATE tracklet_finder.cpp)

target_include_directories(tracklet_finder PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/grain)

target_link_libraries(tracklet_finder ROOT::Core ufw::ufw edep_reader)

install(TARGETS tracklet_finder EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(kalman_filter)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

target_sources(kalman_filter PRIVATE 
              kalman_filter.cpp
              ${HEADERS}
              ${SOURCES})

target_include_directories(kalman_filter PRIVATE . ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(kalman_filter ROOT::Core ufw::ufw edep_reader)

install(TARGETS kalman_filter EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(time_slicing)

include_directories(../../../include)
include_directories(../../common)
include_directories(../../data)
include_directories(.)

file(GLOB SOURCES *.cc)
file(GLOB HEADERS *.hh)

target_sources(time_slicing PRIVATE time_slicing.cpp ${SOURCES})

target_link_libraries(time_slicing ROOT::RIO ufw::ufw)

install(TARGETS time_slicing EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
add_library(grain_digitize)

include_directories(../../../../include)
include_directories(../../../common)
include_directories(.)

target_sources(grain_digitize PRIVATE digitize.hpp digitize.cpp)

target_link_libraries(grain_digitize ufw::ufw common)

install(TARGETS grain_digitize EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

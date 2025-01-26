add_library(produce_srsand)

set(DUNEANAOBJ_PATH ../../../../duneanaobj/)
include_directories(${DUNEANAOBJ_PATH})
include_directories(../../common)
include_directories(../../data/caf_srsand)
include_directories(.)

target_sources(produce_srsand PRIVATE produce_srsand.cpp)

target_link_libraries(produce_srsand common)

install(TARGETS produce_srsand EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

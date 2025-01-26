add_library(caf_srsand)

#Manual dependency until we get a proper cmake
set(DUNEANAOBJ_PATH ../../../../duneanaobj/)
include_directories(${DUNEANAOBJ_PATH})

include_directories(../../common)
include_directories(.)

target_sources(caf_srsand PRIVATE caf_srsand.hpp caf_srsand.cpp)

ROOT_GENERATE_DICTIONARY(G__caf_srsand caf_srsand.hpp MODULE caf_srsand LINKDEF SRSANDLinkDef.h)

target_link_libraries(caf_srsand common)

install(TARGETS caf_srsand EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

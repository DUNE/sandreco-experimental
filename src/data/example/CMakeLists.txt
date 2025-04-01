add_library(example)

include_directories(../../../include)
include_directories(../../common)

target_sources(example PRIVATE example.cpp)

ROOT_GENERATE_DICTIONARY(G__example ../../../include/example.h MODULE example LINKDEF ../../../include/ExampleLinkDef.h)

target_link_libraries(example ROOT::RIO ROOT::Tree ufw::ufw common)

install(TARGETS example EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

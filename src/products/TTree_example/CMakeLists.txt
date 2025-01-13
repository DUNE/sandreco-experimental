add_library(TTree_example)

include_directories(../../../include)
include_directories(../../common)

target_sources(TTree_example PRIVATE TTree_example.cpp)

ROOT_GENERATE_DICTIONARY(ExampleDict ../../../include/example.h MODULE TTree_example LINKDEF ../../../include/ExampleLinkDef.h)

target_link_libraries(TTree_example ROOT::RIO ROOT::Tree Boost::log ufw::ufw common)

install(TARGETS TTree_example EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

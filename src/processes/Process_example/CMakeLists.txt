add_library(Process_example)

include_directories(../../../include)
include_directories(../../common)

target_sources(Process_example PRIVATE Process_example.cpp)

target_link_libraries(Process_example ROOT::RIO ROOT::Tree Boost::log ufw::ufw common)

install(TARGETS Process_example EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

find_package(duneanaobj REQUIRED)

add_library(read_caf read_caf.cpp)

add_dependencies(read_caf caf_streamer)
target_include_directories(read_caf PRIVATE ../../data/common/ ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(read_caf ufw::ufw duneanaobj::StandardRecord)

install(TARGETS read_caf EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
add_library(png_streamer)

find_package(PNG REQUIRED)

target_sources(png_streamer PRIVATE png_streamer.cpp)

target_include_directories(png_streamer PRIVATE . ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(png_streamer PNG::PNG ROOT::Core ufw::ufw)

install(TARGETS png_streamer EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(sand_png_png_streamer)

find_package(PNG REQUIRED)

target_sources(sand_png_png_streamer PRIVATE png_streamer.cpp)

target_include_directories(sand_png_png_streamer PRIVATE . ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(sand_png_png_streamer PUBLIC ufw::ufw PRIVATE PNG::PNG ROOT::Core)

install(TARGETS sand_png_png_streamer EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

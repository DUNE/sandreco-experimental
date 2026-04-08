find_package(PNG REQUIRED)

sandreco_add_streamer(sand_png_png_streamer)

target_sources(sand_png_png_streamer PRIVATE png_streamer.cpp)

target_link_libraries(sand_png_png_streamer PRIVATE PNG::PNG ROOT::Core)

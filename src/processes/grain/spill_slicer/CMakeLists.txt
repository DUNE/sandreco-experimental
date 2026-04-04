sandreco_add_process(sand_grain_spill_slicer)

target_sources(sand_grain_spill_slicer PRIVATE spill_slicer.cpp)

target_include_directories(sand_grain_spill_slicer PRIVATE "${CMAKE_SOURCE_DIR}/src/data/grain")

target_link_libraries(sand_grain_spill_slicer PRIVATE ROOT::Core)

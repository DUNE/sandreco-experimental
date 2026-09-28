sandreco_add_streamer(sand_caf_caf_rntuple_streamer)

target_sources(sand_caf_caf_rntuple_streamer PRIVATE
    caf_rntuple_streamer.hpp
    caf_rntuple_streamer.cpp)

target_link_libraries(sand_caf_caf_rntuple_streamer PRIVATE ROOT::ROOTNTuple duneanaobj::StandardRecord)

sandreco_add_test_process(sand_test_track_hits_fill)

target_sources(sand_test_track_hits_fill PRIVATE track_hits_fill.cpp)

# For the ROOT Math headers pulled in by common/sand.h
target_link_libraries(sand_test_track_hits_fill PRIVATE ROOT::MathMore)

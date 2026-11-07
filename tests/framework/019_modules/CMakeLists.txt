sandreco_add_test_process(sand_test_track_hits_resolve)

target_sources(sand_test_track_hits_resolve PRIVATE track_hits_resolve.cpp)

# For the ROOT Math headers pulled in by common/sand.h
target_link_libraries(sand_test_track_hits_resolve PRIVATE ROOT::MathMore)

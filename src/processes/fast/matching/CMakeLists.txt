sandreco_add_process(sand_fast_matching)

target_sources(sand_fast_matching
  PRIVATE
  matching.cpp
)

target_link_libraries(sand_fast_matching PRIVATE duneanaobj::StandardRecord)

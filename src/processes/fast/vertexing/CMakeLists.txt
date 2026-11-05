sandreco_add_process(sand_fast_vertexing)

target_sources(sand_fast_vertexing
  PRIVATE
  vertexing.cpp
)

target_link_libraries(sand_fast_vertexing PRIVATE duneanaobj::StandardRecord)

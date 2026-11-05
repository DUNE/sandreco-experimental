sandreco_add_process(sand_fast_pid)

target_sources(sand_fast_pid
  PRIVATE
  pid.cpp
)

target_link_libraries(sand_fast_pid PRIVATE duneanaobj::StandardRecord)

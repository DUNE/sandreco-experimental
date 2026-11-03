sandreco_add_process(sand_common_fast_matching)

target_sources(sand_common_fast_matching
  PRIVATE
  fast_matching.cpp
  truth_matching.cpp
  fast_vertexing.cpp
  fast_pid.cpp
  fast_kinematics.cpp
)

target_link_libraries(sand_common_fast_matching PRIVATE ROOT::MathCore duneanaobj::StandardRecord)

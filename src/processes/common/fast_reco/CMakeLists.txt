sandreco_add_process(sand_common_fast_reco)

target_sources(sand_common_fast_reco
  PRIVATE
  fast_reco.cpp
)

target_link_libraries(sand_common_fast_reco PRIVATE ROOT::MathCore)

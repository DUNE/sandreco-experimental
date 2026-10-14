sandreco_add_filter(sand_filter_primary_nu)

target_include_directories(sand_filter_primary_nu PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

target_sources(sand_filter_primary_nu PRIVATE primary_nu.cpp)

target_link_libraries(sand_filter_primary_nu PRIVATE sand_edep_reader sand_genie_reader sand_common_truth_filler ROOT::MathCore)

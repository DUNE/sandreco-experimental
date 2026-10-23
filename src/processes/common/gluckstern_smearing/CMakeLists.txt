sandreco_add_process(sand_common_gluckstern_smearing)

target_sources(sand_common_gluckstern_smearing
  PRIVATE
  gluckstern_smearing.cpp
)

target_include_directories(sand_common_gluckstern_smearing PRIVATE "${CMAKE_SOURCE_DIR}/src/processes/common/")

target_link_libraries(sand_common_gluckstern_smearing
  PRIVATE
  sand_root_tgeomanager
  sand_edep_reader
  sand_common_truth_filler
  ROOT::MathCore
  ROOT::EG
  duneanaobj::StandardRecord
)

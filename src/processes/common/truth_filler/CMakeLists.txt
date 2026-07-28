sandreco_add_process(sand_common_truth_filler)

target_sources(sand_common_truth_filler
    PRIVATE
    truth_filler.cpp
    truth_filler_details.cpp
    evtcode_parser.cpp
)

add_dependencies(sand_common_truth_filler sand_edep_reader sand_genie_reader)
target_link_libraries(sand_common_truth_filler PRIVATE sand_edep_reader sand_genie_reader ROOT::RIO ROOT::Tree ROOT::EG duneanaobj::StandardRecord)

add_library(sand_smeared_fake_reco
    smeared_fake_reco.cpp
    GlucksternSmearing.cpp
    ../caf_handlers/caf_filler_true.cpp
    ../caf_handlers/caf_filler_reco.cpp
    ../caf_handlers/smeared_caf_filler_reco.cpp
    ../genie_helpers/EvtCode_parser.cpp
)

add_dependencies(sand_smeared_fake_reco sand_edep_reader sand_genie_reader sand_caf_caf_streamer)
target_include_directories(sand_smeared_fake_reco PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../../data/common/
    ${CMAKE_SOURCE_DIR}/include
    ${duneanaobj_SOURCE_DIR}
)

target_link_libraries(sand_smeared_fake_reco PUBLIC ufw::ufw PRIVATE ROOT::Core ROOT::EG sand_edep_reader sand_genie_reader sand_caf_caf_streamer duneanaobj::StandardRecord)

install(TARGETS sand_smeared_fake_reco EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
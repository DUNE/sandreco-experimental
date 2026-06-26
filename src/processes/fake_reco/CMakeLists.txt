sandreco_add_process(sand_fake_reco)

target_sources(sand_fake_reco
    PRIVATE
    fake_reco.cpp
    caf_handlers/caf_filler_reco.cpp
)

add_dependencies(sand_fake_reco sand_caf_caf_streamer)

target_link_libraries(sand_fake_reco PRIVATE ROOT::Core ROOT::EG sand_caf_caf_streamer duneanaobj::StandardRecord)

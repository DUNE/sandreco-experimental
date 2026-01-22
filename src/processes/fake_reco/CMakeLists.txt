add_library(fake_reco fake_reco.cpp)

add_dependencies(fake_reco edep_reader genie_reader caf_streamer)
target_include_directories(fake_reco PRIVATE ../../data/common/ ${CMAKE_SOURCE_DIR}/include ${duneanaobj_SOURCE_DIR})

target_link_libraries(fake_reco PRIVATE ufw::ufw edep_reader genie_reader StandardRecord caf_streamer)

install(TARGETS fake_reco EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

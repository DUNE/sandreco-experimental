find_package(duneanaobj REQUIRED)

add_library(fake_reco fake_reco.cpp)

add_dependencies(fake_reco edep_reader)
target_include_directories(fake_reco PRIVATE ../../data/common/ ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(fake_reco ufw::ufw edep_reader duneanaobj::StandardRecord)

install(TARGETS fake_reco EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
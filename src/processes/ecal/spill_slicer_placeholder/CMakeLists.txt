add_library(sand_ecal_spill_slicer_placeholder)

target_sources(sand_ecal_spill_slicer_placeholder PRIVATE spill_slicer_placeholder.cpp)

target_include_directories(sand_ecal_spill_slicer_placeholder PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_ecal_spill_slicer_placeholder PUBLIC ufw::ufw PRIVATE sand_edep_reader sand_root_tgeomanager sand_geoinfo)

install(TARGETS sand_ecal_spill_slicer_placeholder EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

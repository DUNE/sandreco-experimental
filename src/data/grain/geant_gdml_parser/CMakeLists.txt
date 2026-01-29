add_library(sand_grain_geant_gdml_parser)

find_package(Geant4 REQUIRED)

set(HDRS geant_gdml_parser.hpp)

target_sources(sand_grain_geant_gdml_parser PRIVATE
    ${HDRS}
    geant_gdml_parser.cpp)

target_include_directories(sand_grain_geant_gdml_parser PRIVATE .)

target_link_libraries(sand_grain_geant_gdml_parser PUBLIC ufw::ufw ${Geant4_LIBRARIES} PRIVATE z)

install(TARGETS sand_grain_geant_gdml_parser EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(sand_grain_geant_gdml_parser INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

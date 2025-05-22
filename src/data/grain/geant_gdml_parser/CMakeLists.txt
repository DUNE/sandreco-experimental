add_library(geant_gdml_parser)

find_package(Geant4 REQUIRED)

target_sources(geant_gdml_parser PRIVATE
    geant_gdml_parser.hpp
    geant_gdml_parser.cpp)

target_include_directories(geant_gdml_parser PRIVATE .)

target_link_libraries(geant_gdml_parser ufw::ufw ${Geant4_LIBRARIES} z)

install(TARGETS geant_gdml_parser EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

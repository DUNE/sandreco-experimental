add_library(geant_run_manager)

find_package(Geant4 REQUIRED)

target_sources(geant_run_manager PRIVATE
    geant_run_manager.hpp
    geant_run_manager.cpp)

target_include_directories(geant_run_manager PRIVATE .)

target_link_libraries(geant_run_manager ufw::ufw ${Geant4_LIBRARIES} z)

install(TARGETS geant_run_manager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(geant_run_manager)

find_package(Geant4 REQUIRED)

set(HDRS geant_run_manager.hpp)

target_sources(geant_run_manager PRIVATE
    ${HDRS}
    geant_run_manager.cpp)

target_include_directories(geant_run_manager PRIVATE .)

target_link_libraries(geant_run_manager ufw::ufw ${Geant4_LIBRARIES} z)

install(TARGETS geant_run_manager EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(geant_run_manager INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

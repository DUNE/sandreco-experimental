set(HDRS geant_run_manager.hpp)

sandreco_add_complex_data(sand_grain_geant_run_manager EXPORT_HEADERS ${HDRS})

find_package(Geant4 REQUIRED)

target_sources(sand_grain_geant_run_manager PRIVATE geant_run_manager.cpp)

target_link_libraries(sand_grain_geant_run_manager PUBLIC ${Geant4_LIBRARIES} PRIVATE z)

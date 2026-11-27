set(HDRS particle_table.hpp)

sandreco_add_complex_data(sand_particle_table EXPORT_HEADERS ${HDRS})

target_sources(sand_particle_table PRIVATE particle_table.cpp)

target_link_libraries(sand_particle_table PRIVATE ROOT::Core ROOT::EG)

# Optional tables for the "extra_table" configuration key, under ufw-basepath/particle_table/
install(FILES light_nuclei.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/sandreco/data/particle_table")

set(HDRS volumereco_cl_manager.hpp)

sandreco_add_complex_data(sand_grain_volumereco_cl_manager EXPORT_HEADERS ${HDRS})

find_package(Geant4 REQUIRED)

target_sources(sand_grain_volumereco_cl_manager PRIVATE volumereco_cl_manager.cpp)

target_link_libraries(sand_grain_volumereco_cl_manager PUBLIC sand_cl sand_hdf5 PRIVATE sand_geoinfo)

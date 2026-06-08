set(HDRS volumereco_cl_manager.hpp)

sandreco_add_complex_data(volumereco_cl_manager EXPORT_HEADERS ${HDRS})

find_package(Geant4 REQUIRED)

target_sources(volumereco_cl_manager PRIVATE volumereco_cl_manager.cpp)

target_link_libraries(volumereco_cl_manager PUBLIC sand_cl PRIVATE sand_geoinfo sand_hdf5)

set(HDRS hdf5.hpp)

sandreco_add_complex_data(sand_hdf5 EXPORT_HEADERS ${HDRS})

find_package(HDF5 REQUIRED COMPONENTS CXX)

target_sources(sand_hdf5 PRIVATE hdf5.cpp)

target_link_libraries(sand_hdf5 PUBLIC ${HDF5_CXX_LIBRARIES})

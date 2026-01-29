find_package(HDF5 REQUIRED COMPONENTS CXX)

add_library(mask_weights_computation)

target_sources(mask_weights_computation PRIVATE mask_weights_computation.cpp ${SOURCES})

target_include_directories(mask_weights_computation PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(mask_weights_computation ufw::ufw ocl sand_geoinfo hdf5)

install(TARGETS mask_weights_computation EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

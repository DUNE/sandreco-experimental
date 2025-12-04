add_library(hdf5)

find_package(HDF5 REQUIRED COMPONENTS CXX)

target_sources(hdf5 PRIVATE
               hdf5.hpp
               hdf5.cpp)

target_include_directories(hdf5 PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(hdf5 ufw::ufw ${HDF5_CXX_LIBRARIES})

install(TARGETS hdf5 EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

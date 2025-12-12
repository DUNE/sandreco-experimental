add_library(hdf5)

find_package(HDF5 REQUIRED COMPONENTS CXX)

set(HDRS hdf5.hpp)

target_sources(hdf5 PRIVATE
              ${HDRS}
               hdf5.cpp)

target_include_directories(hdf5 PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(hdf5 ufw::ufw ${HDF5_CXX_LIBRARIES})

install(TARGETS hdf5 EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(hdf5 INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

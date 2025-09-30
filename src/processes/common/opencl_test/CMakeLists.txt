add_library(opencl_test)

target_sources(opencl_test PRIVATE opencl_test.cpp ${SOURCES})

target_include_directories(opencl_test PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

find_package(OpenCL REQUIRED)

target_link_libraries(opencl_test ufw::ufw OpenCL)

install(TARGETS opencl_test EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

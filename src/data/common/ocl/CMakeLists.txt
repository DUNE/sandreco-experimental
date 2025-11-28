add_library(ocl)

find_package(OpenCL REQUIRED)

target_sources(ocl PRIVATE
               ocl.hpp
               ocl.cpp)

target_include_directories(ocl PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(ocl ufw::ufw OpenCL)

install(TARGETS ocl EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

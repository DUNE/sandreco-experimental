set(HDRS ocl.hpp)

sandreco_add_complex_data(sand_cl EXPORT_HEADERS ${HDRS})

find_package(OpenCL REQUIRED)

target_sources(sand_cl PRIVATE ocl.cpp)

target_link_libraries(sand_cl PUBLIC OpenCL)

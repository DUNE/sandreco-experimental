add_library(sand_cl)

find_package(OpenCL REQUIRED)

set(HDRS ocl.hpp)

target_sources(sand_cl PRIVATE
              ${HDRS}
               ocl.cpp)

target_include_directories(sand_cl PRIVATE . .. ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(sand_cl PUBLIC ufw::ufw OpenCL)

install(TARGETS sand_cl EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (EXPORT_PRIVATE_INTERFACES)
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${HDRS} DESTINATION "include/sandreco/private/${rel_path}")

    target_include_directories(sand_cl INTERFACE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
endif()

cmake_minimum_required(VERSION 3.23)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tools/cmake")

project(sandreco
  LANGUAGES CXX
  DESCRIPTION "A reconstruction framework for DUNE/ND/SAND"
  VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

option(BUILD_SHARED_LIBS "Build libraries as shared libraries by default." ON)
option(RUN_TESTS_IN_BUILD "Run tests during the build process" ON)
option(BUILD_DOCUMENTATION "Build documentation tree" OFF)

cmake_policy(SET CMP0167 NEW)
find_package(Boost COMPONENTS log REQUIRED)

find_package(ROOT COMPONENTS RIO Tree REQUIRED)

find_package(ufw REQUIRED)

add_subdirectory(src)

if(RUN_TESTS_IN_BUILD)
  add_subdirectory(tests)
endif()

if(BUILD_DOCUMENTATION)
  add_subdirectory(docs)
endif()

install(
  EXPORT sandrecoTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sandreco
  NAMESPACE sandreco::
  FILE sandrecoTargets.cmake
  )

include(CMakePackageConfigHelpers)
configure_package_config_file( 
  "${PROJECT_SOURCE_DIR}/tools/cmake/Config.cmake.in" "sandrecoConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sandreco
  PATH_VARS CMAKE_INSTALL_LIBDIR
  )

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/sandrecoConfigVersion.cmake
  VERSION 0.1.0
  COMPATIBILITY SameMajorVersion
  )

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/sandrecoConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/sandrecoConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/sandreco"
  )

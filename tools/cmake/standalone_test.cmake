enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/tests/standalone")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_BINARY_DIR}/include")

# Data directory of the sandreco image (geometries and test files, the ufw-basepath of the framework tests). It does
# not depend on CMAKE_INSTALL_PREFIX: the CI installs into ./install and removes it before running the tests.
set(SANDRECO_IMAGE_DATA_DIR
    "/usr/local/share/sandreco/data"
    CACHE PATH "Data directory of the sandreco image, read by the standalone tests")

function(add_test_with_libs target)
  target_link_libraries(${target} PRIVATE Boost::unit_test_framework ufw::ufw ${ARGN})
  # Absolute path of tests/data, so that tests do not depend on the directory they are run from
  target_compile_definitions(${target} PRIVATE SAND_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
  target_compile_definitions(${target} PRIVATE SAND_IMAGE_DATA_DIR="${SANDRECO_IMAGE_DATA_DIR}")
  add_test(NAME ${target} COMMAND ${target})
endfunction()

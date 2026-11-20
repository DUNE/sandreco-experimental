enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/tests/standalone")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_BINARY_DIR}/include")

function(add_test_with_libs target)
  target_link_libraries(${target} PRIVATE Boost::unit_test_framework ufw::ufw ${ARGN})
  # Absolute path of tests/data, so that tests do not depend on the directory they are run from
  target_compile_definitions(${target} PRIVATE SAND_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
  add_test(NAME ${target} COMMAND ${target})
endfunction()

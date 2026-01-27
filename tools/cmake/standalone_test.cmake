enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/tests/standalone)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src/)

function(add_test_with_libs target)
  target_link_libraries(${target} PRIVATE Boost::unit_test_framework ${ARGN})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

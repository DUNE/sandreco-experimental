include(${CMAKE_SOURCE_DIR}/tools/cmake/standalone_test.cmake)

# The CI runs the tests without src/ and without the install: read the light-nuclei table from a copy in the build tree
configure_file(${CMAKE_SOURCE_DIR}/src/data/common/particle_table/light_nuclei.txt
               ${CMAKE_CURRENT_BINARY_DIR}/light_nuclei.txt COPYONLY)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)

foreach(testSrc ${TEST_SRCS})
  get_filename_component(testName ${testSrc} NAME_WE)
  add_executable(${testName} ${testSrc})
  add_test_with_libs(${testName} sand_particle_table ROOT::Core ROOT::EG)
  target_compile_definitions(${testName} PRIVATE SAND_PARTICLE_TABLE_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endforeach(testSrc)

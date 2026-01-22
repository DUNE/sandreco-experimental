include(${CMAKE_SOURCE_DIR}/tools/cmake/standalone_test.cmake)

find_package(HDF5 REQUIRED COMPONENTS CXX)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)

foreach(testSrc ${TEST_SRCS})
        get_filename_component(testName ${testSrc} NAME_WE)
        add_executable(${testName} ${testSrc})
        add_test_with_libs(${testName} hdf5)
endforeach(testSrc)

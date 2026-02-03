include(${CMAKE_SOURCE_DIR}/tools/cmake/standalone_test.cmake)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)

include_directories(.)

foreach(testSrc ${TEST_SRCS})
        get_filename_component(testName ${testSrc} NAME_WE)
        add_executable(${testName} ${testSrc})
        add_test_with_libs(${testName} sand_cl)
endforeach(testSrc)

enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories(../include)
include_directories(../src/common)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)

foreach(testSrc ${TEST_SRCS})
        get_filename_component(testName ${testSrc} NAME_WE)
        add_executable(${testName} ${testSrc})
        target_link_libraries(${testName} ROOT::RIO ROOT::Tree Boost::log Boost::unit_test_framework ufw::ufw common)
        add_test(NAME ${testName} COMMAND ${testName})
endforeach(testSrc)


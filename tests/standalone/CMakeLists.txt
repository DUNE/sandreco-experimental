enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS CXX)
find_package(ROOT COMPONENTS MathCore REQUIRED)

include_directories(.)
include_directories(../../include)
include_directories(../../src/)

# Standard tests (hdf5, etc.)
file(GLOB STANDARD_TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)
list(FILTER STANDARD_TEST_SRCS EXCLUDE REGEX "fake_reco.test.cpp")

foreach(testSrc ${STANDARD_TEST_SRCS})
        get_filename_component(testName ${testSrc} NAME_WE)
        add_executable(${testName} ${testSrc})
        target_link_libraries(${testName} Boost::unit_test_framework hdf5)
        add_test(NAME ${testName} COMMAND ${testName})
endforeach(testSrc)

# fake_reco tests - need duneanaobj, ROOT, and data readers
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fake_reco.test.cpp")
        add_executable(fake_reco.test fake_reco.test.cpp)
        target_include_directories(fake_reco.test PRIVATE
                ${duneanaobj_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/src/data/common)
        target_link_libraries(fake_reco.test
                Boost::unit_test_framework
                ROOT::MathCore
                StandardRecord
                edep_reader
                genie_reader
                ufw::ufw)
        add_test(NAME fake_reco.test COMMAND fake_reco.test)
endif()

add_subdirectory(hdf5)
add_subdirectory(ocl)

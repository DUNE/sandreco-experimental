include(${CMAKE_SOURCE_DIR}/tools/cmake/standalone_test.cmake)

find_package(EDepSim NAMES EDepSim REQUIRED)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.test.cpp)

foreach(testSrc ${TEST_SRCS})
    get_filename_component(testName ${testSrc} NAME_WE)
    add_executable(${testName} ${testSrc})
    target_include_directories(${testName} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/processes/fake_reco
        ${CMAKE_SOURCE_DIR}/src/data/common
        ${CMAKE_SOURCE_DIR}/include
        ${duneanaobj_SOURCE_DIR}
    )
    add_test_with_libs(${testName} fake_reco ufw::ufw ROOT::Core ROOT::EG StandardRecord edep_reader genie_reader EDepSim::edepsim_io)
endforeach(testSrc)

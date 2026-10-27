enable_testing()

function(sandreco_add_test_process tgt)
  add_library(${tgt})
  add_dependencies(${tgt} sandreco_version)
  target_include_directories(${tgt}
                             PRIVATE
                             "${CMAKE_CURRENT_SOURCE_DIR}"
                             "${CMAKE_SOURCE_DIR}/include"
                             "${CMAKE_SOURCE_DIR}/include/common"
                             "${CMAKE_BINARY_DIR}/include"  #for version.h
  )
  target_link_libraries(${tgt} PUBLIC ufw::ufw)
  install(TARGETS ${tgt} EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
endfunction()

# Framework tests are parallelized by group, and a group is separated by the first digit.
# It is allowed to have dependencies between tests sharing the same hundreds digit.
# Tests will be executed sequentially within a group, in numerical order.
# Groups should follow this scheme
# 000-099 Common infrastructure
# 100-199 Grain specific
# 200-299 ECAL specific
# 300-399 STT specific
# 400-499 Drift specific
# 500-599 Tracker common
# 600-899 Real global reco
# 900-999 Fake lobal reco

file(GLOB TEST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.json)

set(GROUPS "")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(JSON_NAME ${TEST_FILE} NAME_WE)
    if(JSON_NAME MATCHES "^([0-9]+)")
        set(TEST_ID ${CMAKE_MATCH_1})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_ID}_modules")
            add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${TEST_ID}_modules")
        endif()
        add_test(NAME ${JSON_NAME} COMMAND ufwrun ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE})
        # Collect group IDs
        math(EXPR GROUP_ID "${TEST_ID} / 100 * 100")
        list(APPEND GROUPS "${GROUP_ID}")
    endif()
endforeach(TEST_FILE)

list(REMOVE_DUPLICATES GROUPS)
list(SORT GROUPS)

foreach(GROUP_ID ${GROUPS})
    # Create list of tests for this group
    set(GROUP_TESTS "")
    foreach(TEST_FILE ${TEST_FILES})
        if(TEST_FILE MATCHES "^([0-9]+)")
            set(TEST_ID ${CMAKE_MATCH_1})
            math(EXPR CURRENT_GROUP_ID "${TEST_ID} / 100 * 100")
            if(CURRENT_GROUP_ID EQUAL GROUP_ID)
                get_filename_component(JSON_NAME ${TEST_FILE} NAME_WE)
                list(APPEND GROUP_TESTS ${JSON_NAME})
            endif()
        endif()
    endforeach()

    list(SORT GROUP_TESTS)
    # Set dependencies
    list(LENGTH GROUP_TESTS TEST_COUNT)
    if(TEST_COUNT GREATER 1)
        set(PREV_TEST "")
        foreach(THIS_TEST ${GROUP_TESTS})
            if(NOT PREV_TEST STREQUAL "")
                set_tests_properties(${THIS_TEST} PROPERTIES DEPENDS ${PREV_TEST})
            endif()
            set(PREV_TEST ${THIS_TEST})
        endforeach()
    endif()
endforeach()

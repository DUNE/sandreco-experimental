enable_testing()

#Quick Note on naming convention:
#000-099 Common infrastructure
#100-199 Grain specific
#200-299 ECAL specific
#300-399 STT specific
#400-499 Drift specific
#500-599 Tracker common
#600-999 Global reco

file(GLOB TEST_SCRIPTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.json)

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


foreach(testScript ${TEST_SCRIPTS})
        get_filename_component(scriptname ${testScript} NAME_WE)
        string(REGEX MATCH "^[0-9]+" test_id "${scriptname}")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_id}_modules")
            add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${test_id}_modules")
        endif()
        add_test(NAME ${scriptname} COMMAND ufwrun ${CMAKE_CURRENT_SOURCE_DIR}/${testScript})
endforeach(testScript)

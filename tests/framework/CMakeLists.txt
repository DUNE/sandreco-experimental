enable_testing()

file(GLOB TEST_SCRIPTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.json)

foreach(testScript ${TEST_SCRIPTS})
        get_filename_component(scriptname ${testScript} NAME_WE)
        add_test(NAME ${scriptname} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../../src/ufwrun ${CMAKE_CURRENT_SOURCE_DIR}/${testScript})
endforeach(testScript)

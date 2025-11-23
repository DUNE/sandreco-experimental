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

foreach(testScript ${TEST_SCRIPTS})
        get_filename_component(scriptname ${testScript} NAME_WE)
        add_test(NAME ${scriptname} COMMAND ufwrun ${CMAKE_CURRENT_SOURCE_DIR}/${testScript})
endforeach(testScript)

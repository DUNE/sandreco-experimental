add_library(genie_reader_test)

target_sources(genie_reader_test PRIVATE genie_reader_test.cpp ${SOURCES})

target_include_directories(genie_reader_test PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(genie_reader_test ROOT::Core ufw::ufw genie_reader)

install(TARGETS genie_reader_test EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

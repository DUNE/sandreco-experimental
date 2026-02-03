add_library(sand_common_genie_reader_test)

target_sources(sand_common_genie_reader_test PRIVATE genie_reader_test.cpp ${SOURCES})

target_include_directories(sand_common_genie_reader_test PRIVATE . ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/data/common)

target_link_libraries(sand_common_genie_reader_test PUBLIC ufw::ufw PRIVATE ROOT::Core sand_genie_reader)

install(TARGETS sand_common_genie_reader_test EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

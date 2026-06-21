find_package(Doxygen REQUIRED)

# Build public API documentation with doxygen.
# Configuration is done by editing `Doxyfile.in`
file(GLOB_RECURSE all_public_headers "${CMAKE_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE all_sources "${CMAKE_SOURCE_DIR}/src/processes/*.hpp" "${CMAKE_SOURCE_DIR}/src/processes/*.cpp")

set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

set(MODULES_LIST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/modules.md")

add_custom_command(
    OUTPUT ${MODULES_LIST_FILE}
    COMMAND python "${CMAKE_SOURCE_DIR}/tools/autodoc/cli.py" "${CMAKE_SOURCE_DIR}/src/processes/" "-q" "-l" "${MODULES_LIST_FILE}"
    DEPENDS ${all_sources}
    COMMENT "Generating ${MODULES_LIST_FILE}"
)

add_custom_target(DoxygenDocumentation ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
  DEPENDS
    ${all_public_headers}
    ${all_sources}
    ${DOXYFILE_OUT}
    ${MODULES_LIST_FILE}
  BYPRODUCTS
    ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
  COMMENT
    "Generating documentation"
  VERBATIM)

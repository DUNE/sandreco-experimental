find_package(Doxygen REQUIRED)

# Build public API documentation with doxygen.
# Configuration is done by editing `Doxyfile.in`
file(GLOB_RECURSE all_public_headers ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PROJECT_NAME}/*.h)

set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

add_custom_target(DoxygenDocumentation ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
  DEPENDS
    ${all_public_headers}
    ${DOXYFILE_OUT}
  BYPRODUCTS
    ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
  COMMENT
    "Generating documentation"
  VERBATIM)

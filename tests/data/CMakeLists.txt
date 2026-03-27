file(GLOB DATA RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

set(FILES_TO_INSTALL "")

foreach(f ${DATA})
  if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${f})
    list(APPEND FILES_TO_INSTALL ${f})
  endif()
endforeach()

install(
  FILES ${FILES_TO_INSTALL}
  DESTINATION "${CMAKE_INSTALL_DATADIR}/sandreco/data/test"
)
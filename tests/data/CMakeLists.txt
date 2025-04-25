file(GLOB DATA RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

list(REMOVE_ITEM DATA "CMakeLists.txt")

install(
  FILES ${DATA}
  DESTINATION "${CMAKE_INSTALL_DATADIR}/sandreco/data/test"
  )

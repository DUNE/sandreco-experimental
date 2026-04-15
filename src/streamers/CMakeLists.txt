function(sandreco_add_streamer tgt)
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
  message(VERBOSE "Configuring streamer ${tgt} in ${CMAKE_CURRENT_SOURCE_DIR}")
  install(TARGETS ${tgt} EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
endfunction()

add_subdirectory(caf_streamer)
add_subdirectory(debug_streamer)
add_subdirectory(png)
add_subdirectory(root)

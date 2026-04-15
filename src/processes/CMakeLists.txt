function(sandreco_add_process tgt)
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
  message(VERBOSE "Configuring process ${tgt} in ${CMAKE_CURRENT_SOURCE_DIR}")
  install(TARGETS ${tgt} EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
endfunction()

add_subdirectory(common)
add_subdirectory(fake_reco)
add_subdirectory(grain)
add_subdirectory(stt)
add_subdirectory(drift)
add_subdirectory(ecal)

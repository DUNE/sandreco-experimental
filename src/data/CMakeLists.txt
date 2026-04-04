function(sandreco_add_complex_data tgt)
  set(export_headers_list EXPORT_HEADERS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${export_headers_list}")
  add_library(${tgt})
  add_dependencies(${tgt} sandreco_version)
  file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}/src" ${CMAKE_CURRENT_SOURCE_DIR})
  target_include_directories(${tgt}
                             PUBLIC
                             $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/${rel_path}/..>
                             PRIVATE
                             "${CMAKE_CURRENT_SOURCE_DIR}"
                             "${CMAKE_SOURCE_DIR}/include"
                             "${CMAKE_SOURCE_DIR}/include/common"
                             "${CMAKE_BINARY_DIR}/include"  #for version.h
  )

  target_sources(${tgt} PRIVATE ${arg_EXPORT_HEADERS})
  target_link_libraries(${tgt} PUBLIC ufw::ufw)
  message(VERBOSE "Configuring complex data ${tgt} in ${CMAKE_CURRENT_SOURCE_DIR}")
  install(TARGETS ${tgt} EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
  if (EXPORT_PRIVATE_INTERFACES AND "${arg_EXPORT_HEADERS}")
    message(VERBOSE "Exporting private headers ${arg_EXPORT_HEADERS}")
    install(FILES ${arg_EXPORT_HEADERS} DESTINATION "include/sandreco/private/${rel_path}")
    target_include_directories(${tgt} INTERFACE $<INSTALL_INTERFACE:include/sandreco/private/${rel_path}/..>)
  endif()
endfunction()

add_subdirectory(common)
add_subdirectory(grain)

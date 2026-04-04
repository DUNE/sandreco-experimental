
set(GIT_HASH_FILE "${CMAKE_BINARY_DIR}/git_commit_hash.txt")

add_custom_target(update_git ALL
  COMMAND ${CMAKE_COMMAND} -E echo_append "" > "${GIT_HASH_FILE}.tmp"
  COMMAND ${CMAKE_COMMAND} -E env git rev-parse --short HEAD > "${GIT_HASH_FILE}.tmp" || ${CMAKE_COMMAND} -E echo "unknown" > "${GIT_HASH_FILE}.tmp"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GIT_HASH_FILE}.tmp" "${GIT_HASH_FILE}"
  COMMAND ${CMAKE_COMMAND} -E remove -f "${GIT_HASH_FILE}.tmp"
  COMMENT "Updating Git commit hash"
  VERBATIM
)

set_property(TARGET update_git PROPERTY ADDITIONAL_CLEAN_FILES ${GIT_HASH_FILE})

function(target_add_versioning tgt src_ver gen_ver)
  if(TARGET "${tgt}")
    add_dependencies(${tgt} update_git)

    add_custom_command(
      OUTPUT ${gen_ver}
      COMMAND ${CMAKE_COMMAND}
              -DPROJECT_VERSION="${PROJECT_VERSION}"
              -DGIT_HASH_FILE="${GIT_HASH_FILE}"
              -DSOURCE_VERSION_FILE=${src_ver}
              -DGENERATED_VERSION_FILE=${gen_ver}
              -P "${CMAKE_SOURCE_DIR}/tools/cmake/VersionFileGenerator.cmake"
      DEPENDS "${src_ver}" "${GIT_HASH_FILE}"
    )
  else()
    message(FATAL_ERROR "target '${tgt}' not found")
  endif()
endfunction()

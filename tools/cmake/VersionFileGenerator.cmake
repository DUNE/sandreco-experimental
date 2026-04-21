if(EXISTS "${GIT_HASH_FILE}")
  file(READ "${GIT_HASH_FILE}" _git_hash_raw)
  string(STRIP "${_git_hash_raw}" GIT_COMMIT_HASH)
else()
  execute_process(
    COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

configure_file(${SOURCE_VERSION_FILE} ${GENERATED_VERSION_FILE} @ONLY)

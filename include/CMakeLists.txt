#Separate fake library because the INTERFACE library does not seem to activate the add_custom_command
add_library(sandreco_version)

target_sources(sandreco_version
               PRIVATE
               "${CMAKE_CURRENT_BINARY_DIR}/common/version.h"
               "${CMAKE_CURRENT_SOURCE_DIR}/common/version.cpp")

target_include_directories(sandreco_version PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_add_versioning(sandreco_version
"${CMAKE_CURRENT_SOURCE_DIR}/common/version.h.in"
"${CMAKE_CURRENT_BINARY_DIR}/common/version.h"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/common/version.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sandreco/public/common/"
)

add_library(sandreco INTERFACE)
add_dependencies(sandreco sandreco_version)

file(GLOB_RECURSE SANDRECO_PUBLIC_HDRS "*.h" "*.hpp")

target_include_directories(sandreco INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                           $<INSTALL_INTERFACE:include/sandreco/public>)

target_sources(sandreco
  PUBLIC FILE_SET sandrecoPublicHeaders TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
  ${SANDRECO_PUBLIC_HDRS}
  )

target_link_libraries(sandreco INTERFACE ROOT::MathCore ufw::ufw)

install(
  TARGETS sandreco EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
  TARGETS FILE_SET sandrecoPublicHeaders DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sandreco/public"
)

add_library(sandreco INTERFACE)

file(GLOB_RECURSE SANDRECO_PUBLIC_HDRS "*.h" "*.hpp")

target_include_directories(sandreco INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
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

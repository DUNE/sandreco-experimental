sandreco_add_process(time_based_prior)

target_sources(time_based_prior PRIVATE prior_wrapper.cpp)

target_link_libraries(time_based_prior PRIVATE sand_geoinfo)

set(PYTHON_SOURCES prior.py)

install(FILES ${PYTHON_SOURCES} DESTINATION "${CMAKE_INSTALL_LIBDIR}/sandreco/python/")

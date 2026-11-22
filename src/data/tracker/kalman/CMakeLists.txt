set(HDRS state.hpp field.hpp propagator.hpp measurement.hpp filter.hpp engine.hpp)

sandreco_add_complex_data(sand_tracker_kalman EXPORT_HEADERS ${HDRS})

target_sources(sand_tracker_kalman PRIVATE state.cpp field.cpp propagator.cpp measurement.cpp filter.cpp engine.cpp)

target_link_libraries(sand_tracker_kalman PUBLIC ROOT::MathCore)

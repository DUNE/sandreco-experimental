set(HDRS state.hpp particle.hpp field.hpp propagator.hpp measurement.hpp filter.hpp engine.hpp)

sandreco_add_complex_data(sand_tracker_kalman EXPORT_HEADERS ${HDRS})

target_sources(sand_tracker_kalman
               PRIVATE particle.cpp field.cpp propagator.cpp measurement.cpp filter.cpp engine.cpp)

# CLHEP units and constants (header only), from the Geant4 installation, as in genie_reader. The headers are part of
# the public interface (state.hpp), so the include path is PUBLIC.
find_package(Geant4 REQUIRED)
target_include_directories(sand_tracker_kalman SYSTEM PUBLIC ${Geant4_INCLUDE_DIRS})

target_link_libraries(sand_tracker_kalman PUBLIC ROOT::MathCore PRIVATE ROOT::EG)

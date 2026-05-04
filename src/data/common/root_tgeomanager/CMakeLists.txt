set(HDRS root_tgeomanager.hpp)

sandreco_add_complex_data(sand_root_tgeomanager EXPORT_HEADERS ${HDRS})

target_sources(sand_root_tgeomanager PRIVATE root_tgeomanager.cpp)

target_link_libraries(sand_root_tgeomanager PUBLIC ROOT::Geom ROOT::MathCore ROOT::Matrix PRIVATE ROOT::RIO ROOT::Tree)

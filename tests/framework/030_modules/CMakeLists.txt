sandreco_add_test_process(sand_test_geoinfo)

target_sources(sand_test_geoinfo PRIVATE geoinfo.cpp)

target_link_libraries(sand_test_geoinfo PRIVATE ROOT::Core sand_geoinfo sand_root_tgeomanager)

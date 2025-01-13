add_library(common)

set(COMMON_SRCS
    TFileStreamer.hpp
    TFileStreamer.cpp
    )

include_directories(.)

target_sources(common PRIVATE ${COMMON_SRCS})

target_link_libraries(common ROOT::RIO ROOT::Tree Boost::log ufw::ufw)

install(TARGETS common EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

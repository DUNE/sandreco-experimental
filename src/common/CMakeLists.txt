add_library(common)

set(COMMON_SRCS
    TTree_product.hpp
    TTree_product.cpp
    )

include_directories(.)

target_sources(common PRIVATE ${COMMON_SRCS})

ROOT_GENERATE_DICTIONARY(ExampleDict ../../include/example.h MODULE common LINKDEF ../../include/ExampleLinkDef.h)

target_link_libraries(common ROOT::RIO ROOT::Tree Boost::log ufw::ufw)

install(TARGETS common EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(caf_srinteraction)

#Manual dependency until we get a proper cmake
set(DUNEANAOBJ_PATH ../../../../duneanaobj/)
include_directories(${DUNEANAOBJ_PATH})

include_directories(../../common)

target_sources(caf_srinteraction PRIVATE caf_srinteraction.cpp)

ROOT_GENERATE_DICTIONARY(G__caf_srinteraction ${DUNEANAOBJ_PATH}/duneanaobj/StandardRecord/SRInteraction.h
                         MODULE caf_srinteraction
                         LINKDEF SRInteractionLinkDef.h)

target_link_libraries(caf_srinteraction ROOT::RIO ROOT::Tree Boost::log ufw::ufw common)

install(TARGETS caf_srinteraction EXPORT sandrecoTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})

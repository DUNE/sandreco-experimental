# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sand/sandreco-experimental/_deps/duneanaobj-src"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-build"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/tmp"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/src/duneanaobj-populate-stamp"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/src"
  "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/src/duneanaobj-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/src/duneanaobj-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sand/sandreco-experimental/_deps/duneanaobj-subbuild/duneanaobj-populate-prefix/src/duneanaobj-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

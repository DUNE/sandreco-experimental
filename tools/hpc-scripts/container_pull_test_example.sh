#!/bin/bash

BRANCH_NAME="53-opencl-module" 

cd /tmp
git clone https://anonymous:$GIT_TOKEN@baltig.infn.it/dune/sandreco-experimental.git sandreco
cd sandreco 
git switch $BRANCH_NAME
git pull https://anonymous:$GIT_TOKEN@baltig.infn.it/dune/sandreco-experimental.git 
mkdir -p build && cd build
cmake -S ../ -B . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local/ -DBUILD_DOCUMENTATION=OFF 
make -j8 install 
cd ..

ufwrun test/framework/048_opencl_test_device.json



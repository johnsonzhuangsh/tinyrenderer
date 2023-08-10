#!/bin/bash

# build/ check/create
if [ ! -d "build" ]; then
    mkdir --parents build
fi
cd build

# cmake/make processing/build
cmake ..
make

# return to original path
cd ..
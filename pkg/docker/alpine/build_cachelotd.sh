#!/bin/sh

BUILD_TYPE="Release"

set -eu
git clone https://github.com/cachelot/cachelot.git cachelot
cd cachelot
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
make
./bin/${BUILD_TYPE}/unit_tests
cp -f ./bin/${BUILD_TYPE}/cachelotd ../cachelotd

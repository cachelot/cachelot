#!/bin/bash

if [ -z "$1" ]; then
  echo "No configuration specified. Abort."
  exit 99
fi

PARALLEL="-j1"
if [ TRAVIS == "yes" ]; then
  PARALLEL="-j2"
else
  [ -r "/proc/cpuinfo" ] && PARALLEL="-j$(grep -c '^processor' /proc/cpuinfo)"
fi

echo "Building $1 ..."
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$1 . &&
make ${PARALLEL} || exit 1
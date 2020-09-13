#!/bin/bash

BUILD_CFGS="Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer UBSanitizer"

. ./cleanup.sh
PARALLEL="-j1"
if [ TRAVIS == "yes" ]; then
  PARALLEL="-j2"
else
  [ -r "/proc/cpuinfo" ] && PARALLEL="-j$(grep -c '^processor' /proc/cpuinfo)"
fi
for cfg in ${BUILD_CFGS}; do
  clean_build_files
  echo "Building ${cfg} ..."
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=${cfg} . &&
  make ${PARALLEL} || exit 1
done

#!/bin/bash

BUILD_CFGS="Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer"

. ./cleanup.sh

[ -r "/proc/cpuinfo" ] && PARALLEL="-j$(grep -c '^processor' /proc/cpuinfo)"
for cfg in ${BUILD_CFGS}; do
  clean_build_files
  echo "Building ${cfg} ..."
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=${cfg} &&
  make ${PARALLEL}
  if [[ $? != 0 ]]; then
    echo "*** ERROR: ${cfg} build failed"
    exit 1
  fi
done

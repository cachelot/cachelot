#!/bin/sh

BUILD_CFGS=(Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer)

. ./cleanup.sh

for cfg in ${BUILD_CFGS[@]}; do
  clean_build_files
  echo "Building ${cfg} ..."
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=${cfg} &&
  make -j 2 || exit 1
done


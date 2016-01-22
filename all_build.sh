#!/bin/sh

BUILD_CFGS=(Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer)

. ./cleanup.sh

for cfg in ${BUILD_CFGS[@]}; do
  clean_build_files
  echo "Building ${cfg} ..."
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=${cfg} &&
  make -j 4 &&
  mkdir -p "./bin/${cfg}" &&
  find './bin' -maxdepth 1 -type f -exec mv {} "./bin/${cfg}/" \; &&
  echo "All files in ./bin/${cfg}" || break
done


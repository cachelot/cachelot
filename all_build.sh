#!/bin/bash

BUILD_CFGS="Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer UBSanitizer"

. ./cleanup.sh
for cfg in ${BUILD_CFGS}; do
  clean_build_files
  ./build.sh ${cfg}
done

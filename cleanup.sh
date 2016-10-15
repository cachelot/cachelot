#!/bin/bash

ROOT=$(dirname $(realpath ${BASH_SOURCE[0]}))

function clean_artifacts {
    rm -rf "${ROOT}/bin/*"
    rm -rf "${ROOT}/lib/*"
    rm -rf "${ROOT}/doc/Doxygen/*"
}

function clean_build_files {
    find "${ROOT}" -type d -and \( -name "CMakeFiles" -or -name "CMakeScripts" -or -name "*.build" -or -name "*.xcodeproj" \) | xargs rm -r > /dev/null 2>&1
    find "${ROOT}" -type f -and \( -name "CMakeCache.txt" -or -name "cmake_install.cmake" -or -name "*.pyc" -or -name "*.xctestrun" \) | xargs rm > /dev/null 2>&1
    rm -f Makefile
    rm -f src/*/Makefile
}

clean_artifacts
clean_build_files
echo >&2 "clean"

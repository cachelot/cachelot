#!/bin/sh

function clean_artifacts {
    rm -r ./bin/* > /dev/null 2>&1
    rm -r doc/Doxygen/* > /dev/null 2>&1
}

function clean_build_files {
    find . -type d -and \( -name "CMakeFiles" -or -name "CMakeScripts" -or -name "*.build" -or -name "*.xcodeproj" \) | xargs rm -r > /dev/null 2>&1
    find . -type f -and \( -name "CMakeCache.txt" -or -name "cmake_install.cmake" -or -name "Makefile" -or -name "*.pyc" \) | xargs rm > /dev/null 2>&1
}

clean_artifacts
clean_build_files
echo "Done."

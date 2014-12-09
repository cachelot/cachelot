#!/bin/sh

rm -r ./bin/* > /dev/null 2>&1
find . -type d -name "CMakeFiles" -or -name "CMakeScripts" -or -name "*.build" -or -name "*.xcodeproj" | xargs rm -r > /dev/null 2>&1
find . -type f -name "CMakeCache.txt" -or -name "cmake_install.cmake" -or -name "Makefile" | xargs rm > /dev/null 2>&1
echo "Done."

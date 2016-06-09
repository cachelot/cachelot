#!/bin/sh

REPO="cachelot"
[ -d "${REPO}" ] && rm -rf ${REPO}
git clone https://github.com/cachelot/cachelot.git ${REPO}
cd ${REPO} &&
./all_build.sh &&
./run_tests.sh

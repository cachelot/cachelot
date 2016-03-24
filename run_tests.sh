#!/bin/bash

MYDIR=$(dirname $0)

function server_test {
    local buildCfg="$1"
    ${MYDIR}/bin/"${buildCfg}"/cachelotd &
    local pid=$!
    ${MYDIR}/test/server_test.py
    local ret=$?
    kill ${pid} || ret=$?
    [[ ${ret} != 0 ]] && exit ${ret}
    sleep 0.5  # ensure process is down
}


function run_tests {
    local buildCfg="$1"
    echo "*** [${buildCfg}]"
    bindir="${MYDIR}/bin/${buildCfg}"
    if [ ! -d "${bindir}" ]; then
        echo "*** [ - skipped -]"
        return 0
    fi
    # Run basic smoke tests
    server_test "${buildCfg}"
    # Run other tests/benchmarks depending on build type
    case "${buildCfg}" in
    Debug)
        ./${bindir}/unit_tests || exit 1
        ;;
    Release)
        ./${bindir}/unit_tests || exit 1
        ./${bindir}/benchmark_cache || exit 1
        ./${bindir}/benchmark_memalloc || exit 1
        ;;
    RelWithDebugInfo)
        ./${bindir}/unit_tests || exit 1
        ./${bindir}/benchmark_cache || exit 1
        ./${bindir}/benchmark_memalloc || exit 1
        ;;
    MinSizeRel)
        ./${bindir}/unit_tests || exit 1
        ./${bindir}/benchmark_cache || exit 1
        ./${bindir}/benchmark_memalloc || exit 1
        ;;
    AddressSanitizer)
        ./${bindir}/unit_tests || exit 1
        ./${bindir}/benchmark_cache || exit 1
        ;;
    *)
        echo "Unknown build configuration";
        exit 1;
        ;;
    esac
}

BUILD_CFGS="Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer"

for cfg in ${BUILD_CFGS}; do
    run_tests "${cfg}"
done

echo ""
echo "*** All tests completed successfully"


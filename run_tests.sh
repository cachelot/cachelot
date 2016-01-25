#!/bin/bash


function server_test {
    local buildCfg=$1
    ./bin/"${buildCfg}"/cachelotd &
    sleep 0.5
    ./test/server_test.py
    local ret=$?
    killall cachelotd
    sleep 0.1
    [[ ${ret} != 0 ]] && exit ${ret}
}

echo "*** [Debug]"
server_test Debug
bin/Debug/unit_tests || exit 1

echo "*** [Release]"
server_test Release
bin/Release/unit_tests || exit 1
bin/Release/benchmark_cache || exit 1
bin/Release/benchmark_memalloc || exit 1

echo "*** [RelWithDebugInfo]"
server_test RelWithDebugInfo
bin/RelWithDebugInfo/unit_tests || exit 1
bin/RelWithDebugInfo/benchmark_cache || exit 1
bin/RelWithDebugInfo/benchmark_memalloc || exit 1

echo "*** [MinSizeRel]"
server_test MinSizeRel
bin/MinSizeRel/unit_tests || exit 1
bin/MinSizeRel/benchmark_cache || exit 1
bin/MinSizeRel/benchmark_memalloc || exit 1

echo "*** [AddressSanitizer]"
server_test AddressSanitizer
bin/AddressSanitizer/benchmark_cache || exit 1
bin/AddressSanitizer/unit_tests || exit 1

echo ""
echo "*** All tests completed successfully"


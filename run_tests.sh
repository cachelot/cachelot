#!/bin/bash


function server_test {
    local buildCfg=$1
    ./bin/"${buildCfg}"/cachelotd &
    local pid=$!
    ./test/server_test.py
    local ret=$?
    kill ${pid} || ret=$?
    [[ ${ret} != 0 ]] && exit ${ret}
    sleep 0.5  # ensure process is down
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


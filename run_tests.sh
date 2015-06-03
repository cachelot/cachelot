#!/bin/sh

./bin/Debug/unit_tests
./bin/Release/unit_tests
./bin/Release/benchmark_memalloc
./bin/Release/benchmark_cache
./bin/Release/cachelotd & 
./src/test/server_test.py
killall cachelotd


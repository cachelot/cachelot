project (cachelot_benchmarks)

include (${CMAKE_SOURCE_DIR}/src/cachelot/CMakeLists.txt)


### Cache benchmark
add_executable(cache_benchmark cache_benchmark.cpp ${CACHELOT_SOURCES})

set_target_properties (cache_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
target_link_libraries (cache_benchmark ${Boost_LIBRARIES})

### Cache async API benchmark
add_executable(cache_async_api_benchmark cache_async_api_benchmark.cpp ${CACHELOT_SOURCES})

set_target_properties (cache_async_api_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
target_link_libraries (cache_async_api_benchmark ${Boost_LIBRARIES})

#######

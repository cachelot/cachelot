project (cachelot_benchmarks)

include (${CMAKE_SOURCE_DIR}/src/cachelot/CMakeLists.txt)

### Cache benchmark
add_executable(cache_benchmark cache_benchmark.cpp ${CACHELOT_SOURCES})

set_target_properties (cache_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})
target_link_libraries (cache_benchmark ${Boost_LIBRARIES})

### Cache async API benchmark
add_executable(cache_service_benchmark cache_service_benchmark.cpp ${CACHELOT_SOURCES})

set_target_properties (cache_service_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})
target_link_libraries (cache_service_benchmark ${Boost_LIBRARIES})

#######

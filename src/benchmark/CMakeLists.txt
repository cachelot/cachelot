project (cachelot_benchmarks)

include (${CMAKE_SOURCE_DIR}/src/cachelot/CMakeLists.txt)

### Cache benchmark
add_executable(benchmark_cache benchmark_cache.cpp ${CACHELOT_SOURCES})
set_target_properties (benchmark_cache PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})
target_link_libraries (benchmark_cache ${Boost_LIBRARIES})

### Memalloc benchmark
add_executable(benchmark_memalloc benchmark_memalloc.cpp ${CACHELOT_SOURCES})
set_target_properties (benchmark_memalloc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})
target_link_libraries (benchmark_memalloc ${Boost_LIBRARIES})



#######

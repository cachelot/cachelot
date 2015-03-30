project (cachelot_benchmarks)

include (${CMAKE_SOURCE_DIR}/src/cachelot/CMakeLists.txt)

### Cache benchmark
add_executable(benchmark_cache benchmark_cache.cpp ${CACHELOT_SOURCES})
target_link_libraries (benchmark_cache ${Boost_LIBRARIES})
set_target_properties (benchmark_cache PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})

### Memalloc benchmark
set (TLSF_SOURCES
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.c"
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.h"
    )
include_directories(${INCLUDE_DIRECTORIES} "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src")
add_executable(benchmark_memalloc benchmark_memalloc.cpp ${CACHELOT_SOURCES} ${TLSF_SOURCES})
target_link_libraries (benchmark_memalloc ${Boost_LIBRARIES})
set_target_properties (benchmark_memalloc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CACHELOT_BUILD})



#######

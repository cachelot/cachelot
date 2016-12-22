project (cachelot_benchmarks)

### Cache benchmark
add_executable(benchmark_cache benchmark_cache.cpp)
target_link_libraries (benchmark_cache cachelot ${Boost_LIBRARIES})

if (NOT CMAKE_BUILD_TYPE STREQUAL "AddressSanitizer")
### Memalloc benchmark
set (BENCH_MEMALLOC_SRCS
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.c"
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.h"
            benchmark_memalloc.cpp
    )
if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"))
    set_source_files_properties(${BENCH_MEMALLOC_SRCS} PROPERTIES COMPILE_FLAGS -w)
endif ()
include_directories (${INCLUDE_DIRECTORIES} "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src")
add_executable (benchmark_memalloc ${BENCH_MEMALLOC_SRCS})
target_link_libraries (benchmark_memalloc cachelot ${Boost_LIBRARIES})
endif ()

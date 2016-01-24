project (cachelot_benchmarks)

### Cache benchmark
add_executable(benchmark_cache benchmark_cache.cpp)
target_link_libraries (benchmark_cache cachelot ${Boost_LIBRARIES})

if (NOT CMAKE_BUILD_TYPE STREQUAL "AddressSanitizer")
### Memalloc benchmark
set (TLSF_SOURCES
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.c"
            "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src/tlsf.h"
    )
include_directories(${INCLUDE_DIRECTORIES} "${CMAKE_SOURCE_DIR}/src/thirdparty/TLSF-2.4.6/src")
add_executable(benchmark_memalloc benchmark_memalloc.cpp ${TLSF_SOURCES})
target_link_libraries (benchmark_memalloc cachelot ${Boost_LIBRARIES})
endif ()

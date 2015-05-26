# ${CMAKE_CURRENT_LIST_DIR} allows to include this files anywhere in the project
# For instance it is included in unit tests to simplify linking

set (CACHELOT_SOURCES
    common.h
    common.cpp
    bits.h
    hash_table.h
    bytes.h
    error.h
    error.cpp
    dict.h
    hash_fnv1a.h
    random.h
    debug_trace.h
    memalloc.h
    memalloc-impl.h
    item.h
    cache.h
    cache.cpp
    cache_defs.h
    stats.h
    stats.cpp
)

add_library (cachelot STATIC ${CACHELOT_SOURCES})
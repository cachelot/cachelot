# ${CMAKE_CURRENT_LIST_DIR} allows to include this files anywhere in the project
# For instance it is included in unit tests to simplify linking

set (CACHELOT_SOURCES
    bits.h
    slice.h
    cache.cpp
    cache.h
    cache_defs.h
    common.cpp
    common.h
    debug_trace.h
    dict.h
    error.cpp
    error.h
    expiration_clock.h
    hash_fnv1a.h
    hash_table.h
    intrusive_list.h
    item.h
    memalloc-inl.h
    memalloc.h
    random.h
    stats.cpp
    stats.h
    string_conv.h
    version.h
)

add_library (cachelot STATIC ${CACHELOT_SOURCES})

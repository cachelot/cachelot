#ifndef CACHELOT_STATS_H_INCLUDED
#define CACHELOT_STATS_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup stats Cache usage statistics
/// @{

namespace cachelot {

    #define MEMORY_STATS(X) \
        X(uint64, num_malloc,               "Number of alloc calls") \
        X(uint64, num_free,                 "Number of free calls") \
        X(uint64, num_realloc,              "Number realloc_inplace calls") \
        X(uint64, num_alloc_errors,         "Number of allocation failures") \
        X(uint64, num_realloc_errors,       "Number of realloc failures") \
        X(uint64, total_requested,          "Amount of requested memory") \
        X(uint64, total_served,             "Amount of allocated memory") \
        X(uint64, total_unserved,           "Amount of requested but not allocated due to an error memory") \
        X(uint64, total_realloc_requested,  "Amount of requested memory via realloc calls") \
        X(uint64, total_realloc_served,     "Amount of serverd memory via realloc calls") \
        X(uint64, total_realloc_unserved,   "Amount of requested but not allocated memory via relloc calls") \
        X(uint64, num_free_table_hits,      "Number of times when memory allocated from the corresponding cell of free blocks table") \
        X(uint64, num_free_table_weak_hits, "Number of times when memory allocated from the bigger cell of free blocks table") \
        X(uint64, limit_maxbytes,           "Maximum amount of memory to use for the storage") \
        X(uint64, page_size,                "Size of allocator page (max allocation size)") \
        X(uint64, evictions,                "Number of evicted items")

    #define CACHE_STATS(X) \
        X(uint64, cmd_get,                  "'get' commands") \
        X(uint64, get_hits,                 "'get' cache hits") \
        X(uint64, get_misses,               "'get' cache misses") \
        X(uint64, cmd_set,                  "'set' commands") \
        X(uint64, set_new,                  "'set' inserts ") \
        X(uint64, set_existing,             "'set' updates") \
        X(uint64, cmd_add,                  "'add' commands") \
        X(uint64, add_stored,               "'add' inserts") \
        X(uint64, add_not_stored,           "'add' rejects") \
        X(uint64, cmd_replace,              "'replace' commands") \
        X(uint64, replace_stored,           "'replace' updates") \
        X(uint64, replace_not_stored,       "'replace' cache misses") \
        X(uint64, cmd_cas,                  "'cas' commands") \
        X(uint64, cas_misses,               "'cas' cache misses") \
        X(uint64, cas_stored,               "'cas' updates") \
        X(uint64, cas_badval,               "'cas' reject") \
        X(uint64, cmd_delete,               "'delete' commands") \
        X(uint64, delete_hits,              "'delete' cache hits") \
        X(uint64, delete_misses,            "'delete' cache misses") \
        X(uint64, cmd_touch,                "'touch' commands") \
        X(uint64, touch_hits,               "'touch' cache hits") \
        X(uint64, touch_misses,             "'touch' cache misses") \
        X(uint64, cmd_incr,                 "'incr' commands") \
        X(uint64, incr_hits,                "'incr' cache hits") \
        X(uint64, incr_misses,              "'incr' cache misses") \
        X(uint64, cmd_decr,                 "'decr' commands") \
        X(uint64, decr_hits,                "'decr' cache hits") \
        X(uint64, decr_misses,              "'decr' cache misses") \
        X(uint64, cmd_append,               "'append' commands") \
        X(uint64, append_stored,            "'append' updates") \
        X(uint64, append_misses,            "'append' cache misses") \
        X(uint64, cmd_prepend,              "'prepend' commands") \
        X(uint64, prepend_stored,           "'prepend' updates") \
        X(uint64, prepend_misses,           "'prepend' cache misses") \
        X(uint64, cmd_flush,                "'flush_all' commands") \
        X(uint64, hash_capacity,            "capacity of the hash table") \
        X(uint64, curr_items,               "number of items in the cache") \
        X(bool, hash_is_expanding,          "hash table is expanding")

    struct stats {
        #define DECLARE_STAT(stat_type, stat_name, stat_description) stat_type stat_name = stat_type();

        struct {
            CACHE_STATS(DECLARE_STAT)
        } cache;

        struct {
            MEMORY_STATS(DECLARE_STAT)
        } mem;

        #undef DECLARE_STAT
    };

    /// Print current stat values into stdout
    void PrintStats() noexcept;

    /// Reset all stats to their default values
    void ResetStats() noexcept;

    extern struct stats __stats__;

    #define __STAT2(name) __stats__.name
    #define __STAT(name) __STAT2(name)
    #define __STATGROUP2(group, name) __stats__.group.name
    #define __STATGROUP(group, name) __STATGROUP2(group, name)

    #define STAT_GET(stat_group, stat_name) __STATGROUP(stat_group, stat_name)
    #define STAT_INCR(stat_name, delta) do { __STAT(stat_name) += delta; } while(false)
    #define STAT_DECR(stat_name, delta) do { __STAT(stat_name) = (__STAT(stat_name) >= delta) ? __STAT(stat_name) - delta : 0; } while(false)
    #define STAT_SET(stat_name, value) do { __STAT(stat_name) = value; } while(false)

} // namespace cachelot

/// @}

#endif // CACHELOT_STATS_H_INCLUDED

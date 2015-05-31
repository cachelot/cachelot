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
        X(uint64, num_malloc, 0, "Number of memory allocation requests") \
        X(uint64, num_free, 0, "Number of memory free requests") \
        X(uint64, num_realloc, 0, "Number of memory re-allocation requests") \
        X(uint64, num_alloc_errors, 0, "Number of allocation failures") \
        X(uint64, num_realloc_errors, 0, "Number of realloc failures") \
        X(uint64, total_requested, 0, "Total amount of requested memory") \
        X(uint64, total_served, 0, "Total amount of allocated memory") \
        X(uint64, total_unserved, 0, "Total amount of memory that had requested but not allocated due to an error") \
        X(uint64, total_realloc_requested, 0, "Total amount of memory requested while reallocs") \
        X(uint64, total_realloc_served, 0, "Total amount of memory served to reallocs") \
        X(uint64, total_realloc_unserved, 0, "Total amount of memory that had requested but not allocated due to an error") \
        X(uint64, num_free_table_hits, 0, "Number of times when memory allocated from the corresponding cell of free blocks table") \
        X(uint64, num_free_table_weak_hits, 0, "Number of times when memory allocated from the bigger cell of free blocks table") \
        X(uint64, num_free_table_merges, 0, "Number of times when free blocks have merged into one to satisfy new allocation") \
        X(uint64, num_used_table_hits, 0, "Number of times when memory allocated from the corresponding cell of used blocks table") \
        X(uint64, num_used_table_weak_hits, 0, "Number of times when memory allocated from the bigger cell of used blocks table")  \
        X(uint64, num_used_table_merges, 0, "Number of times when used blocks have merged into one to satisfy new allocation") \
        X(uint64, limit_maxbytes, 0, "Maximum amount of memory to use for the storage")

    #define CACHE_STATS(X) \
        X(uint64, cmd_get, 0, "Number of 'get' commands")           \
        X(uint64, get_hits, 0, "Times when 'get' hit the cache")    \
        X(uint64, get_misses, 0, "Times when 'get' miss the cache") \
        X(uint64, cmd_set, 0, "Number of 'set' commands")           \
        X(uint64, cmd_add, 0, "Number of 'add' commands")           \
        X(uint64, cmd_replace, 0, "Number of 'replace' commands")   \
        X(uint64, cmd_delete, 0, "Number of 'delete' commands")     \
        X(uint64, delete_hits, 0, "Description of the stat")      \
        X(uint64, delete_misses, 0, "Description of the stat")      \
        X(uint64, cmd_cas, 0, "Number of 'cas' commands")           \
        X(uint64, cas_misses, 0, "Description of the stat")         \
        X(uint64, cas_hits, 0, "Description of the stat")           \
        X(uint64, cas_badval, 0, "Description of the stat")         \
        X(uint64, cmd_touch, 0, "Description of the stat")          \
        X(uint64, touch_hits, 0, "Description of the stat")         \
        X(uint64, touch_misses, 0, "Description of the stat")       \
        X(uint64, incr_hits, 0, "Description of the stat")          \
        X(uint64, incr_misses, 0, "Description of the stat")        \
        X(uint64, decr_hits, 0, "Description of the stat")          \
        X(uint64, decr_misses, 0, "Description of the stat")        \
        X(uint64, hash_power_level, 0, "Description of the stat")   \
        X(uint64, hash_bytes, 0, "Description of the stat")         \
        X(uint64, hash_is_expanding, 0, "Description of the stat")  \
        X(uint64, curr_items, 0, "Description of the stat")         \
        X(uint64, total_items, 0, "Description of the stat")        \
        X(uint64, evicted_unfetched, 0, "Description of the stat")  \
        X(uint64, evictions, 0, "Description of the stat")          \
        X(uint64, reclaimed, 0, "Description of the stat")

    struct stats {
        #define DECLARE_STAT(field_type, field_name, field_default, field_description) field_type field_name = field_default;
        struct {
            MEMORY_STATS(DECLARE_STAT)
        } mem;
        struct {
            CACHE_STATS(DECLARE_STAT)
        } cache;
        #undef DECLARE_STAT
    };

    /// Print current stat values into stdout
    void PrintStats();

    extern struct stats __stats__;

    #define __GET_STAT2(name) __stats__.name
    #define __GET_STAT(name) __GET_STAT2(name)

    #define STAT_INCR(stat_name, delta) do { __GET_STAT(stat_name) += delta; } while(false)
    #define STAT_DECR(stat_name, delta) do { __GET_STAT(stat_name) = (__GET_STAT(stat_name) >= delta) ? __GET_STAT(stat_name) - delta : 0; } while(false)


} // namespace cachelot

/// @}

#endif // CACHELOT_STATS_H_INCLUDED

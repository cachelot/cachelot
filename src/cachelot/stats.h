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

    // TODO: Description of the stats

    #define MEMORY_STATS(X) \
        X(uint64, num_malloc, "Number of memory allocation requests") \
        X(uint64, num_free, "Number of memory free requests") \
        X(uint64, num_realloc, "Number of memory re-allocation requests") \
        X(uint64, num_alloc_errors, "Number of allocation failures") \
        X(uint64, num_realloc_errors, "Number of realloc failures") \
        X(uint64, total_requested, "Total amount of requested memory") \
        X(uint64, total_served, "Total amount of allocated memory") \
        X(uint64, total_unserved, "Total amount of memory that had requested but not allocated due to an error") \
        X(uint64, total_realloc_requested, "Total amount of memory requested while reallocs") \
        X(uint64, total_realloc_served, "Total amount of memory served to reallocs") \
        X(uint64, total_realloc_unserved, "Total amount of memory that had requested but not allocated due to an error") \
        X(uint64, num_free_table_hits, "Number of times when memory allocated from the corresponding cell of free blocks table") \
        X(uint64, num_free_table_weak_hits, "Number of times when memory allocated from the bigger cell of free blocks table") \
        X(uint64, num_free_table_merges, "Number of times when free blocks have merged into one to satisfy new allocation") \
        X(uint64, num_used_table_hits, "Number of times when memory allocated from the corresponding cell of used blocks table") \
        X(uint64, num_used_table_weak_hits, "Number of times when memory allocated from the bigger cell of used blocks table")  \
        X(uint64, num_used_table_merges, "Number of times when used blocks have merged into one to satisfy new allocation") \
        X(uint64, limit_maxbytes, "Maximum amount of memory to use for the storage") \
        X(uint64, evictions, "Description of the stat")

    #define CACHE_STATS(X) \
        X(uint64, cmd_get, "Number of 'get' commands")           \
        X(uint64, get_hits, "Times when 'get' hit the cache")    \
        X(uint64, get_misses, "Times when 'get' miss the cache") \
        X(uint64, cmd_set, "Number of 'set' commands")           \
        X(uint64, set_new, "Number of 'set' commands")           \
        X(uint64, set_existing, "Number of 'set' commands")      \
        X(uint64, cmd_add, "Number of 'add' commands")           \
        X(uint64, add_stored, "Number of 'add' commands")        \
        X(uint64, add_not_stored, "Number of 'add' commands")    \
        X(uint64, cmd_replace, "Number of 'replace' commands")   \
        X(uint64, replace_stored, "Number of 'add' commands")    \
        X(uint64, replace_not_stored, "Number of 'add' commands")\
        X(uint64, cmd_cas, "Number of 'cas' commands")           \
        X(uint64, cas_misses, "Description of the stat")         \
        X(uint64, cas_hits, "Description of the stat")           \
        X(uint64, cas_badval, "Description of the stat")         \
        X(uint64, cmd_delete, "Number of 'delete' commands")     \
        X(uint64, delete_hits, "Description of the stat")        \
        X(uint64, delete_misses, "Description of the stat")      \
        X(uint64, cmd_touch, "Description of the stat")          \
        X(uint64, touch_hits, "Description of the stat")         \
        X(uint64, touch_misses, "Description of the stat")       \
        X(uint64, cmd_incr, "Description of the stat")           \
        X(uint64, incr_hits, "Description of the stat")          \
        X(uint64, incr_misses, "Description of the stat")        \
        X(uint64, cmd_decr, "Description of the stat")           \
        X(uint64, decr_hits, "Description of the stat")          \
        X(uint64, decr_misses, "Description of the stat")        \
        X(uint64, cmd_append, "Description of the stat")         \
        X(uint64, append_hits, "Description of the stat")        \
        X(uint64, append_misses, "Description of the stat")      \
        X(uint64, cmd_prepend, "Description of the stat")        \
        X(uint64, prepend_hits, "Description of the stat")       \
        X(uint64, prepend_misses, "Description of the stat")     \
        X(uint64, cmd_flush,   "Description of the stat")        \
        X(uint64, hash_capacity, "Description of the stat")      \
        X(uint64, curr_items, "Description of the stat")         \
        X(bool, hash_is_expanding, "Description of the stat")

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

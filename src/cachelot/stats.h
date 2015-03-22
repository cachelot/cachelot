#ifndef CACHELOT_STATS_H_INCLUDED
#define CACHELOT_STATS_H_INCLUDED

/// @ingroup stats Cache usage statistics
/// @{

namespace cachelot {

    #define MEMORY_STATS(X) \
        X(uint64, num_malloc, 0, "Number of memory allocation requests") \
        X(uint64, num_free, 0, "Number of memory free requests") \
        X(uint64, num_realloc, 0, "Number of memory re-allocation requests") \
        X(uint64, num_errors, 0, "Number of allocation failures") \
        X(uint64, total_requested_mem, 0, "Total amount of requested memory") \
        X(uint64, total_served_mem, 0, "Total amount of allocated memory") \
        X(uint64, total_unserved_mem, 0, "Total amount of memory that had requested but not allocated due to an error") \
        X(uint64, num_free_table_hits, 0, "Number of times when memory allocated from the corresponding cell of free blocks table") \
        X(uint64, num_free_table_weak_hits, 0, "Number of times when memory allocated from the bigger cell of free blocks table") \
        X(uint64, num_used_table_hits, 0, "Number of times when memory allocated from the corresponding cell of used blocks table") \
        X(uint64, num_used_table_weak_hits, 0, "Number of times when memory allocated from the bigger cell of used blocks table")  \
        X(uint64, num_used_table_merges, 0, "Number of times when used blocks have merged into one to satisfy new allocation") 

        struct stats {
            #define MEM_STAT_FIELD(field_type, field_name, field_default, field_description) field_type field_name = field_default;
            struct {
                MEMORY_STATS(MEM_STAT_FIELD)
            } mem;
            #undef MEM_STAT_FIELD
        };

        /// Print current stat values into stdout
        void PrintStats();

        extern struct stats stats;


} // namespace cachelot

/// @}

#endif // CACHELOT_STATS_H_INCLUDED

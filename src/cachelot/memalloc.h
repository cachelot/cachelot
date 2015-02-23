#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#define CACHELOT_MEMALLOC_H_INCLUDED

#include <cachelot/bits.h> // pow2 utils

/// @ingroup common
/// @{

// test case forward declaration to allow test access private memalloc members
namespace { namespace test_memalloc { struct test_block_list; } }

namespace cachelot {

    // constants
    namespace const_ {
        debug_only(static constexpr uint64 DBG_MARKER = 1234567890987654321;)
        debug_only(static constexpr char DBG_FILLER = 'X';)
        // minimal block size is `2^min_power_of_2`
        static constexpr uint32 min_power_of_2 = 6;  // limited to 2^min_power_of_2 >= sizeof(memblock)
        // maximal block size is `2^max_power_of_2`
        static constexpr uint32 max_power_of_2 = 30; // can not be changed (memblock::size is 31 bit)
        static constexpr uint32 num_powers_of_2 = max_power_of_2 - min_power_of_2;
        // number of second level cells per first level cell (first level cells are powers of 2 in range `[min_power_of_2..max_power_of_2]`)
        static constexpr uint32 num_blocks_per_pow2 = 8; // bigger value means less memory overhead but increases table size

        constexpr uint32 sub_block_size_of_pow2(uint32 power) {
            // Formula: (2^(power + 1) - 2^power) / num_blocks_per_pow2)
            // bit magic: x/(2^n) <=> x >> n
            // TODO: check if compiler does this optimization by itself
            return (pow2(power + 1) - pow2(power)) >> log2u(num_blocks_per_pow2);
        }

        // minimal size of block, blocks of `min_block_size` will be provided even on attempt to allocate less memory
        static constexpr uint32 min_block_size = pow2(min_power_of_2);
        // maximal size of block, attempt to allocate more memory than `max_block_size` will fail
        static const uint32 max_block_size = pow2(max_power_of_2) - sub_block_size_of_pow2(max_power_of_2 - 1);
        // Two-level block table is placed in memory as a continuous array of `block_table_size` elements
        static constexpr uint32 block_table_size = num_powers_of_2 * num_blocks_per_pow2;
        // Limit amount of memory that can be allocate in a single request to the memalloc
        static const size_t allocation_limit = max_block_size;
    }

   /**
    * Allocator which is capable to work whithin fixed amount of memory
    *  aims to provide fast allocation / deallocation
    *
    * Limitations:
    *  - memalloc is single threaded by design
    *  - maximal single allocation size is limited to `allocation_limit`
    *  - does not distinguish virtual and physical memory (treat whole given memory as commited) and
    *  - does not give unused memory back to OS (actually it doesn't call malloc / free or equivalents at all,
    *    initial contiguous memory volume 'arena' must be pre-allocated by user)
    *  - contrary to standard malloc implementations allocated memory is aligned to 8 bytes boundary (not 16)
    *
    * Advantages:
    *  - works within fixed amount of user-provided memory
    *  - fast (malloc / free / realloc are O(1) operations)
    *  - has small overhead (only 8 bytes of metadata per allocation)
    */
    class memalloc {
        class block;
        class block_list;
        class group_by_size;
    public:
        static const size_t allocation_limit = const_::allocation_limit;

        /// user must provide contiguous volume of memory to work with
        /// @p arena - pointer to memory to work with
        /// @p arena_size - size of `arena` in bytes
        /// @p evictions - enable / disable evictions of allocated blocks
        explicit memalloc(void * arena, const size_t arena_size) noexcept;

        /// Try to allocate `size` bytes, return `nullptr` on fail
        void * alloc(const size_t size) noexcept {
            return alloc_or_evict(size, false, [=](void *) -> void {});
        }

        /// allocate memory or evict previously allocated block(s) if necessary
        /// @p size - amount of memory in bytes
        /// @p evict_if_necessary - do item eviction if there is no free memory
        /// @p on_free_block - in case of eviction call this function for each freed block
        /// @tparam ForeachFreed - `void on_free(void * ptr)`
        template <typename ForeachFreed>
        void * alloc_or_evict(const size_t size, bool evict_if_necessary = false,
                              ForeachFreed on_free_block = [=](void *) -> void {}) noexcept;

        /// try to extend previously allocated memory up to `new_size`, return `nullptr` on fail
        void * try_realloc_inplace(void * ptr, const size_t new_size) noexcept;

        /// free previously allocated `ptr`
        void free(void * ptr) noexcept;

        /// return size of previously allocate memory including technical alignment bytes
        size_t _reveal_actual_size(void * ptr) const noexcept;

        /// touch previously allocated item to increase it's chance to avoid eviction
        void touch(void * ptr) noexcept;

    private:
        /// check whether given `ptr` whithin arena bounaries and block information can be retrieved from it
        inline bool valid_addr(void * ptr) const noexcept;

        /// coalesce adjacent unused blocks (up to `const_::max_block_size`) and return resulting block
        block * merge_unused(block * block) noexcept;

        // disallow copying
        memalloc(const memalloc &) = delete;
        memalloc & operator=(const memalloc &) = delete;

    private:
        // global arena boundaries
        uint8 const * arena_begin;
        uint8 const * arena_end;
        size_t user_available_memory_size;  // amount of memory excluding internal memalloc structures
        // all memory blocks are located in table, grouped by block size
        group_by_size * free_blocks; // free blocks are stored here
        group_by_size * used_blocks; // used for block eviction
        // stats
        struct {
            uint64 num_malloc;
            uint64 num_free;
            uint64 num_realloc;
            uint64 requested_mem_total;
            uint64 served_mem_total;
            uint64 requested_mem;
            uint64 served_mem;
            uint64 num_block_table_hits;
            uint64 num_block_table_splits;
            uint64 num_block_table_merges;
        } stats;

    private:
        // unit tests
        friend class test_memalloc::test_block_list;
    };


} // namespace cachelot

/// @}

#include <cachelot/memalloc.inl>

#endif // CACHELOT_MEMALLOC_H_INCLUDED

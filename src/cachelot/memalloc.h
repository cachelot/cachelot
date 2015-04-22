#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#define CACHELOT_MEMALLOC_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup common
/// @{

// test case forward declaration to allow test access private memalloc members
namespace { namespace test_memalloc { struct test_block_list; } }

namespace cachelot {

   /**
    * Allocator which works whith fixed amount of pre-allocated memory
    *
    * Advantages:
    *  - allocated blocks can be evicted in order to satisfy new allocations
    *  - fast (malloc / free / realloc are amortized O(1) operations)
    *  - efficiently utilizes CPU cache
    *  - has small overhead (8 bytes of metadata per allocation + alignment bytes)
    *
    * Limitations:
    *  - memalloc is single threaded by design
    *  - maximal single allocation size is limited to `allocation_limit`
    *  - does not distinguish virtual and physical memory (treat whole given memory as commited) and
    *  - does not give unused memory back to OS (actually it doesn't call malloc / free or equivalents at all,
    *    initial contiguous memory volume 'arena' must be pre-allocated by user)
    *  - contrary to standard malloc implementations allocated memory is aligned to sizeof(void *) bytes boundary (not 16)
    *
    * @see @ref memalloc (memalloc.inl) for implementation details
    */
    class memalloc {
        class block;
        class block_list;
        class group_by_size;
    public:
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
                              ForeachFreed on_free_block = [](void *) -> void {}) noexcept;

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

        /// mark block as non-used and coalesce it with adjacent unused blocks
        void unuse(block * & blk) noexcept;

        /// mark block as used and give requested memory to user
        void * checkout(block * blk, const size_t requested_size) noexcept;

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

    private:
        // unit tests
        friend struct test_memalloc::test_block_list;
    };


} // namespace cachelot

/// @}

#include <cachelot/memalloc.inl>

#endif // CACHELOT_MEMALLOC_H_INCLUDED

#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#define CACHELOT_MEMALLOC_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#ifndef CACHELOT_COMMON_H_INCLUDED
#  include <cachelot/common.h>
#endif
#ifndef CACHELOT_BITS_H_INCLUDED
#  include <cachelot/bits.h> // pow2 utils / CLZ
#endif
#ifndef CACHELOT_STATS_H_INCLUDED
#  include <cachelot/stats.h>
#endif
#ifndef CACHELOT_INTRUSIVE_LIST_H_INCLUDED
#  include <cachelot/intrusive_list.h> // pages LRU and free blocks list
#endif

// forward declaration to friend with unit test cases
namespace { namespace test_memalloc {
    /// internal
    struct test_free_blocks_by_size;
    /// internal
    struct test_pages;
} }

/// @ingroup common
/// @{

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
    * @see @ref memalloc (memalloc-inl.h) for implementation details
    */
    class memalloc {
        class pages;
        class block;
        class free_blocks_by_size;
    public:
        /// constructor
        /// @p arena_size - amount of memory in bytes to work with
        /// @p page_size - size of internal allocator page.
        ///                Page size limits single allocation size.
        ///                The less page is, the less items would be evicted when allocator ran out of free memory
        explicit memalloc(const size_t memory_limit, const size_t page_size);

        /// Try to allocate `size` bytes, return `nullptr` on fail
        /// Returned memory is guaranteed to be aligned to the `sizeof(void *)`
        ///
        /// @note it's impossible to allocate more than `page_size`
        /// @see memalloc::memalloc
        void * alloc(size_t size) {
            return alloc_or_evict(size, false, [=](void *) -> void {});
        }

        /// allocate memory or evict previously allocated block(s) if necessary
        /// @p size - amount of memory in bytes
        /// @p evict_if_necessary - do item eviction if there is no free memory
        /// @p on_free_block - in case of eviction call this function for each freed block
        /// @tparam ForeachFreed - `void on_free(void * ptr)`
        template <typename ForeachFreed>
        void * alloc_or_evict(size_t size, bool evict_if_necessary = false,
                              ForeachFreed on_free_block = [](void *) -> void {});

        /// try to extend previously allocated memory up to `new_size`, return `nullptr` on fail
        void * realloc_inplace(void * ptr, const size_t new_size) noexcept;

        /// free previously allocated `ptr`
        void free(void * ptr) noexcept;

        /// return size of previously allocate memory including technical alignment bytes
        size_t reveal_actual_size(void * ptr) const noexcept;

        /// touch previously allocated item to increase it's chance to avoid eviction
        void touch(void * ptr) noexcept;

    private:
        /// check whether given `ptr` whithin arena bounaries and block information can be retrieved from it
        inline bool valid_addr(void * ptr) const noexcept;

        /// coalesce adjacent unused blocks up to max block size and return resulting block
        block * merge_unused(block * block) noexcept;

        /// mark block as non-used and coalesce it with adjacent unused blocks
        void unuse(block * & blk) noexcept;

        /// mark block as used and give requested memory to user
        void * checkout(block * blk, const size_t requested_size) noexcept;

        // disallow copying
        memalloc(const memalloc &) = delete;
        memalloc & operator=(const memalloc &) = delete;

        template <typename ForeachFreed>
        void * alloc_or_evict_impl(size_t size, bool evict_if_necessary = false,
                                   ForeachFreed on_free_block = [](void *) -> void {});

    private:
        // total amount of memory
        const size_t arena_size;
        // size of the single page
        const uint32 page_size;
        // pointer to the memory arena
        std::unique_ptr<void, decltype(&aligned_free)> m_arena;
        // logical pages
        std::unique_ptr<pages> m_pages;
        // free memory blocks are placed in the table, grouped by block size
        std::unique_ptr<free_blocks_by_size> m_free_blocks;
    private:
        // Test cases
        friend struct test_memalloc::test_free_blocks_by_size;
        friend struct test_memalloc::test_pages;
    };

} // namespace cachelot

/// @}

#include <cachelot/memalloc-inl.h>

#endif // CACHELOT_MEMALLOC_H_INCLUDED

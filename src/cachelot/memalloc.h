#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#define CACHELOT_MEMALLOC_H_INCLUDED

#include <cachelot/bits.h> // pow2 utils

/// @ingroup common
/// @{

namespace cachelot {

    // constants
    namespace const_ {
        debug_only(static constexpr uint64 DBG_MARKER = 0x00B00B1E500;)
        // minimal block size is `2**min_power_of_2`
        static constexpr uint32 min_power_of_2 = 6;  // can not be less than sizeof(memblock)
        // maximal block size is `2**max_power_of_2`
        static constexpr uint32 max_power_of_2 = 30; // can not be changed (memblock::size is 31 bit)
        static constexpr uint32 num_powers_of_2 = max_power_of_2 - min_power_of_2;
        // number of second level cells per first level cell (first level cells are powers of 2 in range `[min_power_of_2..max_power_of_2]`)
        static constexpr uint32 num_blocks_per_pow2 = 8; // bigger value means less memory overhead but increases table size

        constexpr uint32 sub_block_size_of_pow2(uint32 power_of_2) {
            // bit magic: x/(2^n) <=> x >> n
            return static_cast<uint32>((pow2(power_of_2 + 1) - pow2(power_of_2)) >> log2u(num_blocks_per_pow2));
        }

        // minimal size of block, blocks of `min_block_size` will be provided even on attempt to allocate less memory
        static constexpr uint32 min_block_size = pow2(min_power_of_2);
        // maximal size of block, attempt to allocate more memory than `max_block_size` will fail
        static const uint32 max_block_size = pow2(max_power_of_2) - sub_block_size_of_pow2(max_power_of_2 - 1);
        // Two-level block table is placed in memory as a continuous array of `block_table_size` elements
        static constexpr uint32 block_table_size = (max_power_of_2 - min_power_of_2) * num_blocks_per_pow2;
        // Limit amount of memory that can be allocate in a single request to the memalloc
        static const size_t allocation_limit = max_block_size;
        // memalloc works within user provided memory *arena*, `min_arena_size` defines minimal size of this arena
        static constexpr size_t min_arena_size = 4096;
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
        class memblock_list;
        class memblock;
        class block_by_size_table;

        /// check whether given `ptr` whithin arena bounaries and block information can be retrieved from it
        inline bool valid_addr(void * ptr) const noexcept;

        /// calculate table indexes (power of 2 index and subblock index) based on `requested_size`
        static tuple<uint32, uint32> block_pos_from_size(const size_t requested_size) noexcept;

        /// coalesce adjacent unused blocks (up to `const_::max_block_size`) and return resulting block
        memblock * merge_unused(memblock * block) noexcept;

        /// try to retrieve available block of `requested_size` from corresponding table cell
        memblock * try_get_free_block(const uint32 requested_size) noexcept;

        /// try to get biggest available block to chop off `requested_size` from it
        memblock * try_get_biggest_free_block(const uint32 requested_size) noexcept;

        /// put unused `block` into corresponding table cell (and update bit indexes)
        void put_free_block(memblock * block) noexcept;

        /// same as put_free_block(), but doesn't need to calculate cell position (used during init)
        void put_biggest_block(memblock * block) noexcept;

        /// retrieve list of blocks by two indexes
        memblock_list & block_list_at(const uint32 power2_index, const uint32 sub_block_index) noexcept;

        /// low-level block retrieval and updating bit indexes
        memblock * retrieve_block_at(const uint32 power2_index, const uint32 sub_block_index) noexcept;

        /// low-level block storage and updating bit indexes
        void store_block_at(memblock * blk, const uint32 power2_index, const uint32 sub_block_index) noexcept;

        /// calculate minimal size of block at given position
        static uint32 block_size_at(const uint32 power2_index, const uint32 sub_block_index) noexcept;

    public:
        static const size_t allocation_limit = const_::allocation_limit;

        /// user must provide contiguous volume of memory to work with
        explicit memalloc(void * arena, const size_t arena_size) noexcept;

        /// try to allocate memory of requested `size`, return `nullptr` on fail
        void * alloc(const size_t size) noexcept;

        /// reallocate previously allocated memory to feet into `new_size`
        void * realloc(void * ptr, const size_t new_size) noexcept;

        /// free previously allocated `ptr`
        void free(void * ptr) noexcept;

        /// return size of previously allocate memory including technical alignment bytes
        size_t _reveal_actual_size(void * ptr) const noexcept;

    private:
        // global arena boundaries
        uint8 const * arena_begin;
        uint8 const * arena_end;
        // all memory blocks are located in table, grouped by block size
        memblock_list * block_table;
        // bit index intends to minimize cache misses
        uint32 second_level_bit_index[const_::num_powers_of_2] = {0};
        uint32 first_level_bit_index = 0;

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
    };

//////// memblock //////////////////////////////////////

    /// single allocation chunk with metadata
    class memalloc::memblock {
        struct {
            bool used : 1;      // indicate whether block is used
            uint32 size : 31;   // amount of memory available to user
            uint32 prev_contiguous_block_offset;  // offset of previous block in continuous arena
            debug_only(uint64 dbg_marker;) // debug constant marker to identify invalid blocks
        } meta;
        struct link_type {      // link for circular list of free blocks
            link_type * prev, * next;
        };

        friend class memalloc::memblock_list;

        union {
            link_type link;
            uint8 memory_[1];  // pointer to actual memory given to user
        };

    public:
        /// public size of block metadata
        constexpr static const uint32 meta_size = sizeof(meta);
        /// value of block alignment
        constexpr static const uint32 alignment = meta_size;
        /// minimal block size to split
        constexpr static const uint32 split_threshold = const_::min_block_size + meta_size;

        /// @name constructors
        ///@{

        /// constructor (used by technical 'border' blocks)
        memblock() noexcept {
            static_assert(unaligned_bytes(memory_, alignof(void *)) == 0, "User memory must be properly aligned");
            meta.used = false;
            meta.size = 0;
            meta.prev_contiguous_block_offset = 0;
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
        }
        /// constructor used for normal blocks
        explicit memblock(const uint32 size, const memblock * prev_contiguous_block) noexcept {
            debug_assert(size >= const_::min_block_size || size == 0);
            debug_assert(size <= const_::max_block_size);
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
            debug_assert(reinterpret_cast<const uint8 *>(prev_contiguous_block) + prev_contiguous_block->size() + meta_size == reinterpret_cast<uint8 *>(this));
            meta.size = size;
            meta.used = false;
            meta.prev_contiguous_block_offset = prev_contiguous_block->size() + meta_size;
        }
        ///@}

        /// amount of memory available to user
        uint32 size() const noexcept { return meta.size; }

        /// amount of memory including metadata
        uint32 size_with_meta() const noexcept { return size() + meta_size; }

        /// modify size stored in block metadata
        void set_size(const uint32 new_size) noexcept {
            debug_assert(new_size <= const_::max_block_size);
            debug_assert(new_size >= const_::min_block_size);
            meta.size = new_size;
        }

        /// check whether block is used
        bool is_used () const noexcept { return meta.used; }

        /// check whether block is free
        bool is_free () const noexcept { return not meta.used; }

        /// block adjacent to the left in arena
        memblock * prev_contiguous() noexcept {
            uint8 * this_ = reinterpret_cast<uint8 *>(this);
            return reinterpret_cast<memalloc::memblock *>(this_ - meta.prev_contiguous_block_offset);
        }

        /// block adjacent to the right in arena
        memblock * next_contiguous() noexcept {
            uint8 * this_ = reinterpret_cast<uint8 *>(this);
            return reinterpret_cast<memalloc::memblock *>(this_ + size_with_meta());
        }

        /// debug only block self check
        bool valid() noexcept {
            debug_assert(this != nullptr); // Yep! It is
            debug_assert(meta.dbg_marker == const_::DBG_MARKER);
            debug_assert(meta.size >= const_::min_block_size || meta.size == 0);
            debug_assert(meta.size <= const_::max_block_size);
            if (meta.size > 0) {
                debug_assert(next_contiguous()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(next_contiguous()->prev_contiguous() == this);
                debug_assert(prev_contiguous()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(prev_contiguous()->next_contiguous() == this);
            }
            return true;
        }

        /// get block structure back from pointer given to user
        static memblock * from_user_ptr(void * ptr) noexcept {
            uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
            memalloc::memblock * result = reinterpret_cast<memalloc::memblock *>(u8_ptr - offsetof(memalloc::memblock, memory_));
            debug_assert(result->meta.dbg_marker == const_::DBG_MARKER);
            return result;
        }

        /// mark block as used and return pointer to available memory to user
        static void * checkout(memblock * block) noexcept {
            debug_assert(block->valid() || block->size() == 0);
            debug_assert(not block->meta.used);
            block->meta.used = true;
            return block->memory_;
        }

        /// mark block as free
        static void unuse(memblock * block) noexcept {
            debug_assert(block->valid());
            debug_assert(block->is_used());
            block->meta.used = false;
        }

        /// split given `block` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<memblock *, memblock *> split(memblock * block, const uint32 new_size) noexcept {
            debug_assert(block->valid());
            debug_assert(block->is_free());
            const uint32 new_round_size = new_size > const_::min_block_size ? new_size + unaligned_bytes(new_size, alignment) : const_::min_block_size;
            debug_assert(new_round_size < block->size());
            memalloc::memblock * leftover = nullptr;
            if (block->size() - new_round_size >= split_threshold) {
                const uint32 old_size = block->size();
                memalloc::memblock * block_after_next = block->next_contiguous();
                block->set_size(new_round_size);
                leftover = new (block->next_contiguous()) memalloc::memblock(old_size - new_round_size - memalloc::memblock::meta_size, block);
                block_after_next->meta.prev_contiguous_block_offset = leftover->size() + memalloc::memblock::meta_size;
                debug_assert(leftover->valid());
                return make_tuple(block, leftover);
            }
            debug_assert(block->valid());
            return make_tuple(block, leftover);
        }

        /// merge two blocks up to `max_block_size`
        static memblock * merge(memblock * left_block, memblock * right_block) noexcept {
            debug_assert(left_block->valid());
            debug_assert(right_block->valid());
            debug_assert(right_block->prev_contiguous() == left_block);
            debug_assert(left_block->next_contiguous() == right_block);
            debug_assert(left_block->is_free()); debug_assert(right_block->is_free());
            debug_assert(left_block->size() + right_block->size() + memalloc::memblock::meta_size < const_::max_block_size);
            memalloc::memblock * block_after_right = right_block->next_contiguous();
            left_block->set_size(left_block->size() + right_block->size() + memalloc::memblock::meta_size);
            block_after_right->meta.prev_contiguous_block_offset = left_block->size() + memalloc::memblock::meta_size;
            return left_block;
        }
    };


//////// memblock_list /////////////////////////////////

    /// double linked circular list of memblock
    class memalloc::memblock_list {
    public:
        /// constructor
        constexpr memblock_list() noexcept : dummy_link({&dummy_link, &dummy_link}) {}

        /// check if list is empty
        bool empty() const noexcept {
            return dummy_link.next == &dummy_link;
        }

        /// add `block` to the list
        void put(memblock * block) noexcept {
            memblock::link_type * link = &block->link;
            link->next = dummy_link.next;
            link->next->prev = link;
            link->prev = &dummy_link;
            dummy_link.next = link;
        }

        /// try to get block from the list, return `nullptr` if list is empty
        memblock * get() noexcept {
            debug_assert(not empty());
            memblock::link_type * link = dummy_link.next;
            unlink(link);
            auto ui8_ptr = reinterpret_cast<uint8 *>(link);
            return reinterpret_cast<memblock *>(ui8_ptr - offsetof(memblock, link));
        }

        /// remove `block` from the list
        static void unlink(memblock * block) noexcept {
            unlink(&block->link);
        }

    private:
        static void unlink(memblock::link_type * link) noexcept {
            debug_assert(link->prev != link && link->next != link);
            link->prev->next = link->next;
            link->next->prev = link->prev;
            debug_only(link->next = link->prev = nullptr);
        }

        memblock::link_type dummy_link;
    };


//////// block_by_size_table ///////////////////////////

    /**
     * Segreagation table of memblock_list, index matches size of stored blocks
     *
     * It is organized as a continuous array where cells are lists of blocks within a size class.
     * Position in table is determined by two indexes:
     *   first index is a power of 2 (32, 64, 128, and so on)
     *   second index sub-divides class size inside neighbord powers of two range [first_lvl_index**2 .. (first_lvl_index + 1)**)
     */
    class block_by_size_table {
    public:
        class iterator {
            iterator(uint32 fl_index, uint32 sl_index) : first_lvl_index(fl_index), second_lvl_index(sl_index) {}
        public:
            constexpr iterator() noexcept = default;
            constexpr iterator(const iterator &) noexcept = default;
            constexpr iterator & operator= (const iterator &) noexcept = default;

            /// increase position by 1
            void increment() noexcept {
                if (second_lvl_index < const_::num_blocks_per_pow2) {
                    second_lvl_index += 1;
                } else {
                    debug_assert(first_lvl_index < const_::num_powers_of_2);
                    first_lvl_index += 1;
                    second_lvl_index = 0;
                }

            }

            /// decrease position by 1
            void decrement() noexcept {
                if (second_lvl_index > 0) {
                    second_lvl_index -= 1;
                } else {
                    debug_assert(first_lvl_index > 0);
                    first_lvl_index -= 1;
                    second_lvl_index = const_::num_blocks_per_pow2 - 1;
                }
            }

            /// calc size class of a corresponding block
            uint32 block_size() const noexcept {
                debug_assert(first_lvl_index < const_::num_powers_of_2);
                debug_assert(second_lvl_index < const_::num_blocks_per_pow2);
                const uint32 power_of_2 = const_::min_power_of_2 + first_lvl_index;
                return pow2(power_of_2) + (second_lvl_index * const_::sub_block_size_of_pow2(power_of_2));
            }

            private:
            friend class block_by_size_table;
            uint32 first_lvl_index;   // corresponding power of 2
            uint32 second_lvl_index;  // index of sub-block within current [first_lvl_index .. next_power_of2) range
        };
    public:
        block_by_size_table();

        uint32 capacity() const noexcept;

        iterator pos_from_size(uint32 requested_size) const noexcept;

        memblock_list & operator[](const iterator pos) noexcept;

        memblock * try_get_block_at(const iterator pos) noexcept;

        memblock * try_get_biggest_block(const uint32 requested_size);

    private:
        // all memory blocks are located in table, segregated by block size
        memblock_list * table;
        // bit indexes intend to minimize cache misses and to speed up search of block of maximal size
        uint8 second_level_bit_index[const_::num_powers_of_2] = {0}; // array index matches first_lvl_index and bits of corresponding cell matches sub-blocks
        uint32 first_level_bit_index = 0;
        static_assert(sizeof(second_level_bit_index[0]) * 8 >= const_::num_blocks_per_pow2, "Not enough bits for sub-blocks");
    };

//////// memalloc //////////////////////////////////////

    inline memalloc::memalloc(void * arena, const size_t arena_size) noexcept {
        static_assert(valid_alignment(memblock::alignment), "memblock::meta must define proper alignment");
        debug_assert(arena_size >= const_::min_arena_size);
        uint8 * available = reinterpret_cast<uint8 *>(arena); // pointer to currently non-used memory
        uint8 * EOM = available + arena_size; // End-Of-Memory marker
        arena_begin = available;
        arena_end = EOM;

        // allocate and init blocks table
        available += unaligned_bytes(available, alignof(memblock_list)); // alignment
        block_table = new (available) memblock_list[const_::block_table_size]();
        available += sizeof(memblock_list) * const_::block_table_size;

        // split all arena on blocks and store them in table

        // first dummy border blocks
        // left border to prevent block merge underflow
        available += unaligned_bytes(available, memblock::alignment); // alignment
        memblock * left_border = new (available) memblock();
        memblock::checkout(left_border); // mark block as used so it will not be merged with others
        available += left_border->size_with_meta();
        // leave space to 'right border' that will prevent block merge overflow
        EOM -= memblock::meta_size;
        EOM -= unaligned_bytes(EOM, memblock::alignment);

        // allocate blocks of maximal size first further they could be split
        memblock * prev_allocated_block = left_border;
        while (static_cast<size_t>(EOM - available) >= const_::max_block_size + memblock::meta_size) {
            memblock * huge_block = new (available) memblock(const_::max_block_size, prev_allocated_block);
            put_biggest_block(huge_block);
            available += huge_block->size_with_meta();
            prev_allocated_block = huge_block;
        }
        // allocate space that's left
        const uint32 leftover_size = EOM - available - memblock::meta_size;
        if (leftover_size >= memblock::split_threshold) {
            memblock * leftover_block = new (available) memblock(leftover_size, prev_allocated_block);
            put_free_block(leftover_block);
            prev_allocated_block = leftover_block;
        } else {
            EOM = available;
        }
        memblock * right_border = new (EOM) memblock(0, prev_allocated_block);
        memblock::checkout(right_border);
    }


    inline void * memalloc::alloc(const size_t size) noexcept {
        debug_assert(size <= const_::allocation_limit);
        debug_assert(size > 0);
        const uint32 nsize = static_cast<const uint32>(size);
        // 1. Try to get block of corresponding size from table
        memblock * free_block = try_get_free_block(nsize);
        if (free_block != nullptr) {
            return memblock::checkout(free_block);
        }
        // 2. Try to split the biggest available block
        memblock * big_block = try_get_biggest_free_block(nsize);
        if (big_block != nullptr) {
            debug_assert(big_block->size() >= nsize);
            memblock * leftover;
            tie(free_block, leftover) = memblock::split(big_block, nsize);
            if (leftover) {
                put_free_block(leftover);
            }
            return memblock::checkout(free_block);
        }
        return nullptr;
    }


    inline void * memalloc::realloc(void * ptr, const size_t new_size) noexcept {
        return nullptr;
    }


    inline void memalloc::free(void * ptr) noexcept {
        debug_assert(valid_addr(ptr));
        memblock * block = memblock::from_user_ptr(ptr);
        memblock::unuse(block);
        // try to merge it
        block = merge_unused(block);
        put_free_block(block);
    }


    inline size_t memalloc::_reveal_actual_size(void * ptr) const noexcept {
        debug_assert(valid_addr(ptr));
        memblock * block = memblock::from_user_ptr(ptr);
        return block->size();
    }


    inline memalloc::memblock * memalloc::try_get_free_block(const uint32 requested_size) noexcept {
        uint32 power2_index, sub_block_index;
        tie(power2_index, sub_block_index) = block_pos_from_size(requested_size);
        if (bit::isset(first_level_bit_index, power2_index) && bit::isset(second_level_bit_index[power2_index], sub_block_index)) {
            return retrieve_block_at(power2_index, sub_block_index);
        }
        return nullptr;
    }


    inline memalloc::memblock * memalloc::try_get_biggest_free_block(const uint32 requested_size) noexcept {
        if (first_level_bit_index > 0) {
            const uint32 power2_index = bit::most_significant(first_level_bit_index);
            if (second_level_bit_index[power2_index] > 0) {
                const uint32 sub_block_index = bit::most_significant(second_level_bit_index[power2_index]);
                if (block_size_at(power2_index, sub_block_index) >= requested_size) {
                    return retrieve_block_at(power2_index, sub_block_index);
                }
            }
        }
        return nullptr;
    }


    inline void memalloc::put_free_block(memalloc::memblock * block) noexcept {
        debug_assert(block->size() >= const_::min_block_size);
        debug_assert(block->size() <= const_::max_block_size);
        //debug_assert(block->valid());
        uint32 power2_index, sub_block_index;
        tie(power2_index, sub_block_index) = block_pos_from_size(block->size());
        // block_pos_from_size() calculates position of block that is satsfied (>= of) requested size,
        // this block can be smaller and hence stored right before calculated position
        if (block->size() < block_size_at(power2_index, sub_block_index)) {
            // following is position (pow2_idx, subblock_idx) decrement
            if (sub_block_index > 0) {
                sub_block_index -= 1;
            } else {
                debug_assert(power2_index > 0);
                power2_index -= 1;
                sub_block_index = const_::num_blocks_per_pow2 - 1;
            }
        }
        store_block_at(block, power2_index, sub_block_index);
    }


    inline void memalloc::put_biggest_block(memblock * block) noexcept {
        debug_assert(block->size() == const_::max_block_size);
        store_block_at(block, const_::num_powers_of_2 - 1, const_::num_blocks_per_pow2 - 1);
    }


    inline tuple<uint32, uint32> memalloc::block_pos_from_size(const size_t requested_size) noexcept {
        debug_assert(requested_size > 0 && requested_size <= const_::max_block_size);
        const uint32 size = requested_size > const_::min_block_size ? static_cast<uint32>(requested_size) : const_::min_block_size;
        const uint32 power = log2u(size);
        debug_assert(power >= const_::min_power_of_2);
        const uint32 remainder = size & (pow2(power) - 1); // same as size % pow2(power);
        const uint32 sub_block_size = const_::sub_block_size_of_pow2(power);
        uint32 power2_idx = power - const_::min_power_of_2;
        debug_assert(power2_idx < const_::num_powers_of_2);
        uint32 sub_block_idx = (remainder + sub_block_size - 1) >> log2u(sub_block_size); // avoid division: x/(2^n) <=> x >> n
        debug_assert(sub_block_idx <= const_::num_blocks_per_pow2);
        if (sub_block_idx == const_::num_blocks_per_pow2) {
            power2_idx += 1;
            debug_assert(power2_idx < const_::num_powers_of_2);
            sub_block_idx = 0;
        }
        return make_tuple(power2_idx, sub_block_idx);
    }


    inline memalloc::memblock * memalloc::merge_unused(memalloc::memblock * block) noexcept {
        while ((block->size() < const_::max_block_size) && block->prev_contiguous()->is_free()) {
            if (block->size() + block->prev_contiguous()->size() + memblock::meta_size < const_::max_block_size) {
                memblock_list::unlink(block->prev_contiguous());
                block = memblock::merge(block->prev_contiguous(), block);
            } else {
                break;
            }
        }
        while ((block->size() < const_::max_block_size) && block->next_contiguous()->is_free()) {
            if (block->size() + block->next_contiguous()->size() + memblock::meta_size < const_::max_block_size) {
                memblock_list::unlink(block->next_contiguous());
                block = memblock::merge(block, block->next_contiguous());
            } else {
                break;
            }
        }
        return block;
    }


    inline memalloc::memblock_list & memalloc::block_list_at(const uint32 power2_index, const uint32 sub_block_index) noexcept {
        const uint32 absolute_table_pos = power2_index * const_::num_blocks_per_pow2 + sub_block_index;
        debug_assert(absolute_table_pos < const_::block_table_size);
        return block_table[absolute_table_pos];
    }


    inline memalloc::memblock * memalloc::retrieve_block_at(const uint32 power2_index, const uint32 sub_block_index) noexcept {
        memalloc::memblock_list & target_list = block_list_at(power2_index, sub_block_index);
        memalloc::memblock * result_block = nullptr;
        // blocks can be removed skipping bit index update (during merge) so this check is absolutely necessary
        if (not target_list.empty()) {
            result_block = target_list.get();
        }
        if (target_list.empty()) {
            // we must update bit indexes if there are no free blocks
            second_level_bit_index[power2_index] = bit::unset(second_level_bit_index[power2_index], sub_block_index);
            if (second_level_bit_index[power2_index] == 0) {
                first_level_bit_index = bit::unset(first_level_bit_index, power2_index);
            }
        }
        return result_block;
    }


    inline void memalloc::store_block_at(memalloc::memblock * blk, const uint32 power2_index, const uint32 sub_block_index) noexcept {
        debug_assert(blk != nullptr);
        debug_assert(block_size_at(power2_index, sub_block_index) <= blk->size());
        memalloc::memblock_list & target_list = memalloc::block_list_at(power2_index, sub_block_index);
        target_list.put(blk);
        // unconditionally update bit index
        second_level_bit_index[power2_index] = bit::set(second_level_bit_index[power2_index], sub_block_index);
        first_level_bit_index = bit::set(first_level_bit_index, power2_index);
    }


    inline uint32 memalloc::block_size_at(const uint32 power2_index, const uint32 sub_block_index) noexcept {
        debug_assert(power2_index < const_::num_powers_of_2);
        debug_assert(sub_block_index < const_::num_blocks_per_pow2);
        const uint32 power_of_2 = const_::min_power_of_2 + power2_index;
        return pow2(power_of_2) + (sub_block_index * const_::sub_block_size_of_pow2(power_of_2));
    }


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
        if (arena_begin < u8_ptr && u8_ptr < arena_end) {
            return memblock::from_user_ptr(ptr)->valid(); // meta.dbg_marker check
        } else {
            return false;
        }
    }


} // namespace cachelot

/// @}

#endif // CACHELOT_MEMALLOC_H_INCLUDED

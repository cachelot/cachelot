#include <cachelot/cachelot.h>
#include <cachelot/memalloc.h>

namespace cachelot {

//////// memblock //////////////////////////////////////

    /// single allocation chunk with metadata
    class memalloc::memblock {
        struct {
            bool used : 1;      // indicate whether block is used
            uint32 size : 31;   // amount of memory available to user
            uint32 prev_contiguous_block_offset;  // offset of previous block in continuous arena
            debug_only(uint64 dbg_marker;) // debug constant marker to identify invalid blocks
        } meta;

        // link for circular list of free blocks
        struct link_type {
            link_type * prev, * next;
        };

        union {
            link_type link;
            uint8 memory_[1];  // pointer to actual memory given to user
        };

        friend class memalloc::memblock_list;

    public:
        /// public size of block metadata
        constexpr static const uint32 meta_size = sizeof(meta);
        /// value of block alignment
        constexpr static const uint32 alignment = sizeof(meta);
        /// minimal block size to split
        constexpr static const uint32 split_threshold = const_::min_block_size + meta_size;

        /// @name constructors
        ///@{

        /// constructor (used by technical 'border' blocks)
        memblock() noexcept {
            meta.used = false;
            meta.size = 0;
            meta.prev_contiguous_block_offset = 0;
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
        }

        /// constructor used for normal blocks
        explicit memblock(const uint32 size, const memblock * prev_contiguous_block) noexcept {
            // User memory must be properly aligned
            debug_assert(unaligned_bytes(memory_, alignof(void *)) == 0);
            // check size
            debug_assert(size >= const_::min_block_size || size == 0);
            debug_assert(size <= const_::max_block_size);
            // set debug marker
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
            meta.size = size;
            meta.used = false;
            meta.prev_contiguous_block_offset = prev_contiguous_block->size_with_meta();
            // check previous block for consistency
            debug_only(prev_contiguous_block->test_check());
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

        /// check whether block is technical border block
        bool is_technical() const noexcept { return size() == 0; }

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
        void test_check() const noexcept {
            debug_assert(this != nullptr); // Yep! It is
            debug_assert(meta.dbg_marker == const_::DBG_MARKER);
            debug_assert(meta.size >= const_::min_block_size || is_technical());
            debug_assert(meta.size <= const_::max_block_size);
            if (not is_technical()) {
                // allow test_check() to be `const`
                memblock * non_const = const_cast<memblock *>(this);
                debug_assert(non_const->next_contiguous()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(non_const->next_contiguous()->prev_contiguous() == this);
                debug_assert(non_const->prev_contiguous()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(non_const->prev_contiguous()->next_contiguous() == this);
            }
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
            debug_only(block->test_check());
            debug_assert(not block->meta.used);
            block->meta.used = true;
            debug_only(std::memset(block->memory_, 0x0C, block->size()));
            return block->memory_;
        }

        /// mark block as free
        static void unuse(memblock * block) noexcept {
            debug_only(block->test_check());
            debug_assert(block->is_used());
            block->meta.used = false;
            debug_only(std::memset(block->memory_, 0x0D, block->size()));
        }

        /// split given `block` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<memblock *, memblock *> split(memblock * block, const uint32 new_size) noexcept {
            debug_only(block->test_check());
            debug_assert(block->is_free());
            const uint32 new_round_size = new_size > const_::min_block_size ? new_size + unaligned_bytes(new_size + meta_size, alignment) : const_::min_block_size;
            debug_assert(new_round_size < block->size());
            memalloc::memblock * leftover = nullptr;
            if (block->size() - new_round_size >= split_threshold) {
                const uint32 old_size = block->size();
                memalloc::memblock * block_after_next = block->next_contiguous();
                block->set_size(new_round_size);
                leftover = new (block->next_contiguous()) memalloc::memblock(old_size - new_round_size - memalloc::memblock::meta_size, block);
                block_after_next->meta.prev_contiguous_block_offset = leftover->size() + memalloc::memblock::meta_size;
                debug_only(leftover->test_check());
                return make_tuple(block, leftover);
            }
            debug_only(block->test_check());
            return make_tuple(block, leftover);
        }

        /// merge two blocks up to `max_block_size`
        static memblock * merge(memblock * left_block, memblock * right_block) noexcept {
            debug_only(left_block->test_check());
            debug_only(right_block->test_check());
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

    private:
        memblock::link_type dummy_link;
    };


//////// block_by_size_table ///////////////////////////

    /**
     * Segreagation table of memblock_list, index matches size of stored blocks
     *
     * It is organized as a continuous array where cells are lists of blocks within a size class.
     * There are two bit indexes intended to minimize cache misses and to speed up search of block of maximal size.
     * In each bitmask: set bit indicates that there is *probably* block of corresponding size
     * unset bit indicates that there is *definitely* no blocks of corresponding size
     *  - Each bit in first level index `pow_index` refers group of size classes of corresponding power of 2
     * @code
       // depends on min_power_of_2
       0000 0001 => [32..64)
       0000 0010 => [64..128)
       // ... and so on until max_power_of_2
     * @endcode
     *  - Each bit of second level index refers to the corresponding size class (sub-block in power of 2)
     * @code
     *  [0000 0000][0000 0001] => 32 // zero sub-block of min_power_of_2
     *  [0000 0000][0000 0010] => 36 // 1st sub-block of min_power_of_2
     *  [0000 0000][0000 0100] => 40 // 2nd sub-block
     *  // ...
     *  [0000 0001][0000 0000] => 64 // zero sub-block of next power of 2
     *  // and so on until max_block_size
     * @endcode
     */
    class memalloc::block_by_size_table {
    public:
        /**
         * position in table
         */
        class iterator {
            explicit constexpr iterator(const uint32 power_index, const uint32 sub_block_index)
                : pow_index(power_index), sub_index(sub_block_index) {
                debug_only(test_check());
            }
        public:
            iterator() noexcept { /* do nothing */ };
            constexpr iterator(const iterator &) noexcept = default;
            iterator & operator= (const iterator &) noexcept = default;

            /// increase position by 1
            void increment() noexcept {
                if (sub_index < const_::num_blocks_per_pow2 - 1) {
                    sub_index += 1;
                } else {
                    debug_assert(pow_index < const_::num_powers_of_2 - 1);
                    pow_index += 1;
                    sub_index = 0;
                }
            }

            /// decrease position by 1
            void decrement() noexcept {
                if (sub_index > 0) {
                    sub_index -= 1;
                } else {
                    debug_assert(pow_index > 0);
                    pow_index -= 1;
                    sub_index = const_::num_blocks_per_pow2 - 1;
                }
            }

            /// calc size class of a corresponding block
            uint32 block_size() const noexcept {
                debug_only(test_check());
                const uint32 power_of_2 = const_::min_power_of_2 + pow_index;
                uint32 size = pow2(power_of_2) + (sub_index * const_::sub_block_size_of_pow2(power_of_2));
                debug_assert(const_::min_block_size <= size && size <= const_::max_block_size);
                return size;
            }

            /// return maximal available position
            static constexpr iterator max() noexcept {
                return iterator(const_::num_powers_of_2 - 1, const_::num_blocks_per_pow2 - 1);
            }

        private:
            /// calculate index of target list in the linear table
            uint32 absolute() const noexcept {
                const uint32 abs_index = pow_index * const_::num_blocks_per_pow2 + sub_index;
                debug_assert(abs_index < const_::block_table_size);
                return abs_index;
            }

            // debug only self-check
            void test_check() const noexcept {
                debug_assert(pow_index < const_::num_powers_of_2);
                debug_assert(sub_index < const_::num_blocks_per_pow2);
                debug_assert(absolute() < const_::block_table_size);
            }

            // debug only check if size within this iterator size class
            void test_size_check(const uint32 size) const noexcept {
                debug_assert(size >= block_size());
                const auto last_pos = iterator::max();
                if ((pow_index < last_pos.pow_index) || (pow_index == last_pos.pow_index && sub_index < last_pos.sub_index)) {
                    iterator this_plus_one = *this;
                    this_plus_one.increment();
                    debug_assert(size < this_plus_one.block_size());
                } else {
                    debug_assert(pow_index == last_pos.pow_index && sub_index == last_pos.sub_index);
                    debug_assert(size == const_::max_block_size);
                    debug_assert(size == block_size());
                }
            }

        private:
            /// corresponding power of 2
            uint32 pow_index;
            /// index of sub-block within current [pow_index .. next_power_of_2) range
            uint32 sub_index;

            friend class block_by_size_table;
        };

        iterator search_pos(const uint32 requested_size) const noexcept {
            debug_assert(const_::min_block_size <= requested_size && requested_size <= const_::max_block_size);
            const uint32 log_num_blocks_per_pow2 = log2u(const_::num_blocks_per_pow2);
            const uint32 power = log2u(requested_size);
            debug_assert(power > log_num_blocks_per_pow2);
            // we need to roundup size as target block may be bigger to satisfy requested_size
            const uint32 roundup_size = requested_size + pow2(power - log_num_blocks_per_pow2) - 1;
            uint32 power2_idx = log2u(roundup_size);
            debug_assert(power2_idx > log_num_blocks_per_pow2);
            uint32 sub_block_idx = (roundup_size >> (power2_idx - log_num_blocks_per_pow2)) - const_::num_blocks_per_pow2;
            power2_idx = power2_idx - const_::min_power_of_2;
            return iterator(power2_idx, sub_block_idx);
        }

        iterator insert_pos(const uint32 requested_size) const noexcept {
            debug_assert(const_::min_block_size <= requested_size && requested_size <= const_::max_block_size);
            // we don't need to roundup size on insert
            uint32 power2_idx = log2u(requested_size);
            debug_assert(power2_idx > log2u(const_::num_blocks_per_pow2));
            uint32 sub_block_idx = (requested_size >> (power2_idx - log2u(const_::num_blocks_per_pow2))) - const_::num_blocks_per_pow2;
            power2_idx = power2_idx - const_::min_power_of_2;
            return iterator(power2_idx, sub_block_idx);
        }

    public:
        block_by_size_table() = default;

        /// capacity of the table
        uint32 capacity() const noexcept { return const_::block_table_size; };

        /// try to retrieve block at position `pos`, return `nullptr` if there are no blocks
        memblock * block_at(const iterator pos) noexcept {
            debug_only(pos.test_check());
            memblock * block;
            memblock_list & list = table[pos.absolute()];
            if (not list.empty()) {
                block = list.get();
                debug_only(block->test_check());
                debug_only(pos.test_size_check(block->size()));
            } else {
                block = nullptr;
            }
            // we must update bit indexes if there are no free blocks left
            if (list.empty()) {
                // 1. update second level index
                second_level_bit_index[pos.pow_index] = bit::unset(second_level_bit_index[pos.pow_index], pos.sub_index);
                // 2. update first level index (only if all corresponding powers of 2 are empty)
                if (second_level_bit_index[pos.pow_index] == 0) {
                    first_level_bit_index = bit::unset(first_level_bit_index, pos.pow_index);
                }
            }
            return block;
        }

        /// try to get block of a `requested_size`, return `nullptr` on fail
        memblock * try_get_block(const uint32 requested_size) noexcept {
            debug_assert(requested_size <= const_::max_block_size);
            iterator pos = search_pos(requested_size);
            // access indexes first, unset bits clearly indicate that corresponding cell is empty
            if (bit::isset(first_level_bit_index, pos.pow_index) && bit::isset(second_level_bit_index[pos.pow_index], pos.sub_index)) {
                return block_at(pos);
            } else {
                return nullptr;
            }
        }

        /// try to get the biggest available block, at least of `requested_size`, return `nullptr` on fail
        memblock * try_get_biggest_block(const uint32 requested_size) noexcept {
            debug_assert(requested_size <= const_::max_block_size);
            // accessing indexes first
            if (first_level_bit_index > 0) {
                // 1. retrieve maximal non-empty cells group among powers of 2
                const uint32 pow_index = bit::most_significant(first_level_bit_index);
                // 2. retrieve the biggest block among group sub-blocks
                debug_assert(second_level_bit_index[pow_index] > 0);
                const uint32 sub_index = bit::most_significant(second_level_bit_index[pow_index]);
                const iterator biggest_block_pos = iterator(pow_index, sub_index);
                // there is biggest block, but does it have sufficient size?
                if (biggest_block_pos.block_size() >= requested_size) {
                    return block_at(biggest_block_pos);
                }
            }
            return nullptr;
        }

        /// store `block` at position `pos`
        void put_block_at(memblock * block, const iterator pos) noexcept {
            debug_only(block->test_check());
            debug_only(pos.test_size_check(block->size()));
            debug_only(pos.test_check());
            memalloc::memblock_list & target_list = table[pos.absolute()];
            target_list.put(block);
            // unconditionally update bit index
            second_level_bit_index[pos.pow_index] = bit::set(second_level_bit_index[pos.pow_index], pos.sub_index);
            first_level_bit_index = bit::set(first_level_bit_index, pos.pow_index);
        }

        /// store `block` at position corresponding to its size
        void put_block(memblock * block) {
            debug_assert(not block->is_technical());
            debug_only(block->test_check());
            iterator pos = insert_pos(block->size());
            put_block_at(block, pos);
        }

        /// store `block` of a maximal available size at the end of the table
        void put_biggest_block(memblock * block) noexcept {
            debug_assert(block->size() == const_::max_block_size);
            debug_assert(iterator::max().block_size() == block->size());
            put_block_at(block, iterator::max());
        }

    private:
        // all memory blocks are located in table, segregated by block size
        memblock_list table[const_::block_table_size];


        uint8 second_level_bit_index[const_::num_powers_of_2] = {0};
        // Each power of 2 (with all sub-blocks) has corresponding bit in `first_level_bit_index`, so it masks `second_level_bit_index` too
        uint32 first_level_bit_index = 0;
        static_assert(sizeof(second_level_bit_index[0]) * 8 >= const_::num_blocks_per_pow2, "Not enough bits for sub-blocks");

        friend class memalloc;
    };

//////// memalloc //////////////////////////////////////

    memalloc::memalloc(void * arena, const size_t arena_size, bool allow_evictions) noexcept {
        static_assert(valid_alignment(memblock::alignment), "memblock::meta must define proper alignment");
        const size_t min_arena_size = sizeof(block_by_size_table) * 2 + sizeof(memblock) * 2 + 1024;
        debug_assert(arena_size >= min_arena_size);
        // pointer to currently non-used memory
        uint8 * available = reinterpret_cast<uint8 *>(arena);
        // End-Of-Memory marker
        uint8 * EOM = available + arena_size;
        arena_begin = available;
        arena_end = EOM;

        // allocate and init blocks table
        auto allocate_block_table = [&]() -> block_by_size_table * {
            // properly align memory
            available +=  unaligned_bytes(available, alignof(block_by_size_table));
            debug_assert(available < EOM);
            auto table = new (available) block_by_size_table();
            available += sizeof(block_by_size_table);
            debug_assert(available < EOM);
            return table;
        };

        free_blocks = allocate_block_table();
        if (allow_evictions) {
            used_blocks = allocate_block_table();
        } else {
            used_blocks = nullptr;
        }

        // split all arena on blocks and store them in free blocks table

        // first dummy border blocks
        // left technical border block to prevent block merge underflow
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
            // temporarily create block on the right to pass block consistency test
            debug_only(new (huge_block->next_contiguous()) memblock(0, huge_block));
            // store block at max pos of table
            free_blocks->put_biggest_block(huge_block);
            available += huge_block->size_with_meta();
            prev_allocated_block = huge_block;
        }
        // allocate space what's left
        const uint32 leftover_size = EOM - available - memblock::meta_size;
        if (leftover_size >= memblock::split_threshold) {
            memblock * leftover_block = new (available) memblock(leftover_size, prev_allocated_block);
            // temporarily create block on the right to pass block consistency test
            debug_only(new (leftover_block->next_contiguous()) memblock(0, leftover_block));
            free_blocks->put_block(leftover_block);
            prev_allocated_block = leftover_block;
        } else {
            EOM = available;
        }
        // right technical border block to prevent block merge overflow
        memblock * right_border = new (EOM) memblock(0, prev_allocated_block);
        memblock::checkout(right_border);
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


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
        if (arena_begin < u8_ptr && u8_ptr < arena_end) {
            debug_only(memblock::from_user_ptr(ptr)->test_check());
            return true;
        } else {
            return false;
        }
    }


    void * memalloc::alloc(const size_t size) noexcept {
        debug_assert(size <= const_::allocation_limit);
        debug_assert(size > 0);
        const uint32 nsize = size > const_::min_block_size ? static_cast<uint32>(size) : const_::min_block_size;
        // 1. Try to get block of corresponding size from table
        memblock * free_block = free_blocks->try_get_block(nsize);
        if (free_block != nullptr) {
            return memblock::checkout(free_block);
        }
        // 2. Try to split the biggest available block
        memblock * big_block = free_blocks->try_get_biggest_block(nsize);
        if (big_block != nullptr) {
            debug_assert(big_block->size() >= nsize);
            memblock * leftover;
            tie(free_block, leftover) = memblock::split(big_block, nsize);
            if (leftover) {
                free_blocks->put_block(leftover);
            }
            return memblock::checkout(free_block);
        }
        return nullptr;
    }


    void * memalloc::try_realloc_inplace(void * ptr, const size_t new_size) noexcept {
        debug_assert(valid_addr(ptr));
        debug_assert(new_size <= const_::allocation_limit);
        debug_assert(new_size > 0);
        memblock * block = memblock::from_user_ptr(ptr);

        if (block->size() >= new_size) {
            return block;
        }
        uint32 available_on_the_right = 0;
        uint32 neccassary = new_size - block->size();
        memblock * b = block;
        while (available_on_the_right < neccassary && b->next_contiguous()->is_free()) {
            available_on_the_right += b->next_contiguous()->size_with_meta();
            b = b->next_contiguous();
        }
        if (available_on_the_right >= neccassary) {
            while (block->size() < new_size) {
                block = memblock::merge(block, block->next_contiguous());
            }
            debug_assert(block->size() >= new_size);
            memblock * leftover;
            tie(block, leftover) = memblock::split(block, new_size);
            if (leftover != nullptr) {
                free_blocks->put_block(leftover);
            }
            return block;
        } else {
            // we don't have enough memory to extend block up to `new_size`
            return nullptr;
        }
    }


    void memalloc::free(void * ptr) noexcept {
        debug_assert(valid_addr(ptr));
        memblock * block = memblock::from_user_ptr(ptr);
        memblock::unuse(block);
        // try to merge it
        block = merge_unused(block);
        free_blocks->put_block(block);
    }


    size_t memalloc::_reveal_actual_size(void * ptr) const noexcept {
        debug_assert(valid_addr(ptr));
        memblock * block = memblock::from_user_ptr(ptr);
        return block->size();
    }


} // namespace cachelot


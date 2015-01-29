#include <cachelot/cachelot.h>
#include <cachelot/memalloc.h>

namespace cachelot {

//////// block //////////////////////////////////////


    /// single allocation chunk with metadata
    class memalloc::block {
        struct {
            uint32 size : 31;   // amount of memory available to user
            bool used : 1;      // indicate whether block is used
            uint32 left_adjacent_offset;  // offset of previous block in continuous arena
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

        friend class memalloc::block_list;

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
        block() noexcept {
            meta.used = false;
            meta.size = 0;
            meta.left_adjacent_offset = 0;
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
        }

        /// constructor used for normal blocks
        explicit block(const uint32 sz, const block * left_adjacent_block) noexcept {
            // User memory must be properly aligned
            debug_assert(unaligned_bytes(memory_, alignof(void *)) == 0);
            // check size
            debug_assert(size >= const_::min_block_size || size == 0);
            debug_assert(size <= const_::max_block_size);
            // set debug marker
            debug_only(meta.dbg_marker = const_::DBG_MARKER);
            // Fill-in memory with debug pattern
            debug_only(std::memset(memory_, const_::DBG_FILLER, size));
            meta.size = sz;
            meta.used = false;
            meta.left_adjacent_offset = left_adjacent_block->size_with_meta();
            // check previous block for consistency
            debug_only(left_adjacent_block->test_check());
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
        block * left_adjacent() noexcept {
            uint8 * this_ = reinterpret_cast<uint8 *>(this);
            return reinterpret_cast<memalloc::block *>(this_ - meta.left_adjacent_offset);
        }

        /// block adjacent to the right in arena
        block * right_adjacent() noexcept {
            uint8 * this_ = reinterpret_cast<uint8 *>(this);
            return reinterpret_cast<memalloc::block *>(this_ + size_with_meta());
        }

        /// debug only block self check
        void test_check() const noexcept {
            debug_assert(this != nullptr); // Yep! It is
            debug_assert(meta.dbg_marker == const_::DBG_MARKER);
            debug_assert(meta.size >= const_::min_block_size || is_technical());
            debug_assert(meta.size <= const_::max_block_size);
            if (not is_technical()) {
                // allow test_check() to be `const`
                debug_only(block * non_const = const_cast<block *>(this));
                debug_assert(non_const->right_adjacent()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(non_const->right_adjacent()->left_adjacent() == this);
                debug_assert(non_const->left_adjacent()->meta.dbg_marker == const_::DBG_MARKER);
                debug_assert(non_const->left_adjacent()->right_adjacent() == this);
            }
        }

        /// get block structure back from pointer given to user
        static block * from_user_ptr(void * ptr) noexcept {
            uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
            memalloc::block * result = reinterpret_cast<memalloc::block *>(u8_ptr - offsetof(memalloc::block, memory_));
            debug_assert(result->meta.dbg_marker == const_::DBG_MARKER);
            return result;
        }

        /// mark block as used and return pointer to available memory to user
        static void * checkout(block * blk) noexcept {
            debug_only(blk->test_check());
            debug_assert(not blk->meta.used);
            blk->meta.used = true;
            // ensure that user memory is still filled with debug pattern
            debug_only(uint32 half_size = blk->size() > 0 ? (blk->size() - sizeof(link)) / 2 : 0;)
            debug_assert(std::memcmp(blk->memory_ + sizeof(link), blk->memory_ + sizeof(link) + half_size, half_size) == 0);
            return blk->memory_;
        }

        /// mark block as free
        static void unuse(block * blk) noexcept {
            debug_only(blk->test_check());
            debug_assert(blk->is_used());
            blk->meta.used = false;
            debug_only(std::memset(blk->memory_, const_::DBG_FILLER, blk->size()));
        }

        /// split given `block` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<block *, block *> split(block * blk, const uint32 new_size) noexcept {
            debug_only(blk->test_check());
            debug_assert(blk->is_free());
            const uint32 new_round_size = new_size > const_::min_block_size ? new_size + unaligned_bytes(new_size + meta_size, alignment) : const_::min_block_size;
            debug_assert(new_round_size < blk->size());
            memalloc::block * leftover = nullptr;
            if (blk->size() - new_round_size >= split_threshold) {
                const uint32 old_size = blk->size();
                memalloc::block * block_after_next = blk->right_adjacent();
                blk->set_size(new_round_size);
                leftover = new (blk->right_adjacent()) memalloc::block(old_size - new_round_size - memalloc::block::meta_size, blk);
                block_after_next->meta.left_adjacent_offset = leftover->size_with_meta();
                debug_only(leftover->test_check());
                return make_tuple(blk, leftover);
            }
            debug_only(blk->test_check());
            return make_tuple(blk, leftover);
        }

        /// merge two blocks up to `max_block_size`
        static block * merge(block * left_block, block * right_block) noexcept {
            debug_only(left_block->test_check());
            debug_only(right_block->test_check());
            debug_assert(right_block->left_adjacent() == left_block);
            debug_assert(left_block->right_adjacent() == right_block);
            debug_assert(left_block->is_free()); debug_assert(right_block->is_free());
            debug_assert(left_block->size() + right_block->size_with_meta() < const_::max_block_size);
            memalloc::block * block_after_right = right_block->right_adjacent();
            left_block->set_size(left_block->size() + right_block->size_with_meta());
            debug_only(std::memset(right_block, const_::DBG_FILLER, sizeof(block)));
            block_after_right->meta.left_adjacent_offset = left_block->size_with_meta();
            return left_block;
        }
    };


//////// block_list /////////////////////////////////

    /// double linked circular list of blocks
    /// It allows to insert uninited nodes and remove nodes outside of list (@see block_list::unlink())
    class memalloc::block_list {
    public:
        /// constructor
        constexpr block_list() noexcept : dummy_link({&dummy_link, &dummy_link}) {}

        /// check if list is empty
        bool empty() const noexcept {
            return dummy_link.next == &dummy_link;
        }

        /// add `block` to the list
        void put(block * blk) noexcept {
            block::link_type * link = &blk->link;
            link->next = dummy_link.next;
            link->next->prev = link;
            link->prev = &dummy_link;
            dummy_link.next = link;
        }

        /// try to get block from the list, return `nullptr` if list is empty
        block * get() noexcept {
            debug_assert(not empty());
            block::link_type * link = dummy_link.next;
            unlink(link);
            auto ui8_ptr = reinterpret_cast<uint8 *>(link);
            return reinterpret_cast<block *>(ui8_ptr - offsetof(block, link));
        }

        /// remove `block` from the list
        static void unlink(block * blk) noexcept {
            unlink(&blk->link);
        }

    private:
        static void unlink(block::link_type * link) noexcept {
            debug_assert(link->prev != link && link->next != link);
            link->prev->next = link->next;
            link->next->prev = link->prev;
            debug_only(link->next = link->prev = nullptr);
        }

    private:
        block::link_type dummy_link;
    };


//////// group_by_size ///////////////////////////

    /**
     * Segreagation table of block_list, index matches size of stored blocks
     *
     * It is organized as a continuous array where cells are lists of blocks within a size class.
     * There are two bit indexes intended to minimize cache misses and to speed up search of block of maximal size.
     * In each bitmask: set bit indicates that there is *probably* block of corresponding size
     * unset bit indicates that there is *definitely* no blocks of corresponding size
     *  - Each bit in first level index `pow_index` refers group of size classes of corresponding power of 2
     * @code
     *  // depends on min_power_of_2
     *  0000 0001 => [32..64)
     *  0000 0010 => [64..128)
     *  // ... and so on until max_power_of_2
     * @endcode
     *  - Each bit of second level index refers to the corresponding size class (sub-block in power of 2)
     * @code
     *  [0000 0000][0000 0001] => [32..36) // zero sub-block of min_power_of_2
     *  [0000 0000][0000 0010] => [36..40) // 1st sub-block of min_power_of_2
     *  [0000 0000][0000 0100] => [40..44) // 2nd sub-block
     *  // ...
     *  [0000 0001][0000 0000] => [64..72) // zero sub-block of next power of 2
     *  // and so on until max_block_size
     * @endcode
     */
    class memalloc::group_by_size {
    public:
        /**
         * position in table
         */
        class position {
            explicit constexpr position(const uint32 power_index, const uint32 sub_block_index)
                : pow_index(power_index), sub_index(sub_block_index) {
                debug_only(test_check());
            }
        public:
            position() noexcept { /* do nothing */ };
            constexpr position(const position &) noexcept = default;
            position & operator= (const position &) noexcept = default;

            /// calc size class of a corresponding block
            uint32 block_size() const noexcept {
                debug_only(test_check());
                const uint32 power_of_2 = const_::min_power_of_2 + pow_index;
                uint32 size = pow2(power_of_2) + (sub_index * const_::sub_block_size_of_pow2(power_of_2));
                debug_assert(const_::min_block_size <= size && size <= const_::max_block_size);
                return size;
            }

            /// return maximal available position
            static constexpr position max() noexcept {
                return position(const_::num_powers_of_2 - 1, const_::num_blocks_per_pow2 - 1);
            }

        private:
            /// calculate index of target list in the linear table
            uint32 absolute() const noexcept {
                const uint32 abs_index = pow_index * const_::num_blocks_per_pow2 + sub_index;
                debug_assert(abs_index < const_::block_table_size);
                return abs_index;
            }

            // debug only return next position
            position next() const noexcept {
                uint32 next_pow_index, next_sub_index;
                if (sub_index < const_::num_blocks_per_pow2 - 1) {
                    next_pow_index = pow_index;
                    next_sub_index = sub_index + 1;
                } else {
                    debug_assert(pow_index < const_::num_powers_of_2 - 1);
                    next_pow_index = pow_index + 1;
                    next_sub_index = 0;
                }
                return position(next_pow_index, next_sub_index);
            }

            // debug only self-check
            void test_check() const noexcept {
                debug_assert(pow_index < const_::num_powers_of_2);
                debug_assert(sub_index < const_::num_blocks_per_pow2);
                debug_assert(absolute() < const_::block_table_size);
            }

            // debug only check if size within this position size class
            void test_size_check(const uint32 size) const noexcept {
                debug_assert(size >= block_size());
                const auto last_pos = position::max();
                if ((pow_index < last_pos.pow_index) || (pow_index == last_pos.pow_index && sub_index < last_pos.sub_index)) {
                    debug_assert(size < next().block_size());
                } else {
                    debug_assert(pow_index == last_pos.pow_index && sub_index == last_pos.sub_index);
                    debug_assert(size == const_::max_block_size);
                    debug_assert(size == block_size());
                }
                (void) size;
            }

        private:
            /// corresponding power of 2
            uint32 pow_index;
            /// index of sub-block within current [pow_index .. next_power_of_2) range
            uint32 sub_index;

            friend class group_by_size;
        };

        position search_pos(const uint32 requested_size) const noexcept {
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
            return position(power2_idx, sub_block_idx);
        }

        position insert_pos(const uint32 requested_size) const noexcept {
            debug_assert(const_::min_block_size <= requested_size && requested_size <= const_::max_block_size);
            // we don't need to roundup size on insert
            uint32 power2_idx = log2u(requested_size);
            debug_assert(power2_idx > log2u(const_::num_blocks_per_pow2));
            uint32 sub_block_idx = (requested_size >> (power2_idx - log2u(const_::num_blocks_per_pow2))) - const_::num_blocks_per_pow2;
            power2_idx = power2_idx - const_::min_power_of_2;
            return position(power2_idx, sub_block_idx);
        }

    public:
        group_by_size() = default;

        /// try to get block of a `requested_size`, return `nullptr` on fail
        block * try_get_block(const uint32 requested_size) noexcept {
            debug_assert(requested_size <= const_::max_block_size);
            position pos = search_pos(requested_size);
            // access indexes first, unset bits clearly indicate that corresponding cell is empty
            if (bit::isset(first_level_bit_index, pos.pow_index) && bit::isset(second_level_bit_index[pos.pow_index], pos.sub_index)) {
                return block_at(pos);
            } else {
                return nullptr;
            }
        }

        /// try to get the biggest available block, at least of `requested_size`, return `nullptr` on fail
        block * try_get_biggest_block(const uint32 requested_size) noexcept {
            debug_assert(requested_size <= const_::max_block_size);
            // accessing indexes first
            if (first_level_bit_index > 0) {
                // 1. retrieve maximal non-empty cells group among powers of 2
                const uint32 pow_index = bit::most_significant(first_level_bit_index);
                // 2. retrieve the biggest block among group sub-blocks
                debug_assert(second_level_bit_index[pow_index] > 0);
                const uint32 sub_index = bit::most_significant(second_level_bit_index[pow_index]);
                const position biggest_block_pos = position(pow_index, sub_index);
                // there is biggest block, but does it have sufficient size?
                if (biggest_block_pos.block_size() >= requested_size) {
                    return block_at(biggest_block_pos);
                }
            }
            return nullptr;
        }

        /// store `block` at position corresponding to its size
        void put_block(block * blk) {
            debug_assert(not blk->is_technical());
            debug_only(blk->test_check());
            position pos = insert_pos(blk->size());
            put_block_at(blk, pos);
        }

        /// store `block` of a maximal available size at the end of the table
        void put_biggest_block(block * blk) noexcept {
            debug_assert(blk->size() == const_::max_block_size);
            debug_assert(position::max().block_size() == blk->size());
            put_block_at(blk, position::max());
        }

    private:
        /// try to retrieve block at position `pos`, return `nullptr` if there are no blocks
        block * block_at(const position pos) noexcept {
            debug_only(pos.test_check());
            block * blk;
            block_list & cell = table[pos.absolute()];
            if (not cell.empty()) {
                blk = cell.get();
                debug_only(blk->test_check());
                debug_only(pos.test_size_check(blk->size()));
            } else {
                blk = nullptr;
            }
            // we must update bit indexes if there are no free blocks left
            if (cell.empty()) {
                // 1. update second level index
                second_level_bit_index[pos.pow_index] = bit::unset(second_level_bit_index[pos.pow_index], pos.sub_index);
                // 2. update first level index (only if all corresponding powers of 2 are empty)
                if (second_level_bit_index[pos.pow_index] == 0) {
                    first_level_bit_index = bit::unset(first_level_bit_index, pos.pow_index);
                }
            }
            return blk;
        }

        /// store `block` at position `pos`
        void put_block_at(block * blk, const position pos) noexcept {
            debug_only(blk->test_check());
            debug_only(pos.test_size_check(blk->size()));
            debug_only(pos.test_check());
            memalloc::block_list & target_list = table[pos.absolute()];
            target_list.put(blk);
            // unconditionally update bit index
            second_level_bit_index[pos.pow_index] = bit::set(second_level_bit_index[pos.pow_index], pos.sub_index);
            first_level_bit_index = bit::set(first_level_bit_index, pos.pow_index);
        }

    private:
        // all memory blocks are located in table, segregated by block size
        block_list table[const_::block_table_size];


        uint8 second_level_bit_index[const_::num_powers_of_2] = {0};
        // Each power of 2 (with all sub-blocks) has corresponding bit in `first_level_bit_index`, so it masks `second_level_bit_index` too
        uint32 first_level_bit_index = 0;
        static_assert(sizeof(second_level_bit_index[0]) * 8 >= const_::num_blocks_per_pow2, "Not enough bits for sub-blocks");

        friend class memalloc;
    };

//////// memalloc //////////////////////////////////////

    memalloc::memalloc(void * arena, const size_t arena_size, bool allow_evictions) noexcept {
        static_assert(valid_alignment(block::alignment), "block::meta must define proper alignment");
        debug_only(const size_t min_arena_size = sizeof(group_by_size) * 2 + sizeof(block) * 2 + 1024);
        debug_assert(arena_size >= min_arena_size);
        // pointer to currently non-used memory
        uint8 * available = reinterpret_cast<uint8 *>(arena);
        // End-Of-Memory marker
        uint8 * EOM = available + arena_size;
        arena_begin = available;
        arena_end = EOM;

        // allocate and init blocks table
        auto allocate_block_table = [&]() -> group_by_size * {
            // properly align memory
            available +=  unaligned_bytes(available, alignof(group_by_size));
            debug_assert(available < EOM);
            auto table = new (available) group_by_size();
            available += sizeof(group_by_size);
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
        available += unaligned_bytes(available, block::alignment); // alignment
        block * left_border = new (available) block();
        block::checkout(left_border); // mark block as used so it will not be merged with others
        available += left_border->size_with_meta();
        // leave space to 'right border' that will prevent block merge overflow
        EOM -= block::meta_size;
        EOM -= unaligned_bytes(EOM, block::alignment);

        // allocate blocks of maximal size first further they could be split
        block * prev_allocated_block = left_border;
        while (static_cast<size_t>(EOM - available) >= const_::max_block_size + block::meta_size) {
            block * huge_block = new (available) block(const_::max_block_size, prev_allocated_block);
            // temporarily create block on the right to pass block consistency test
            debug_only(new (huge_block->right_adjacent()) block(0, huge_block));
            // store block at max pos of table
            free_blocks->put_biggest_block(huge_block);
            available += huge_block->size_with_meta();
            debug_assert(available < EOM);
            prev_allocated_block = huge_block;
        }
        // allocate space what's left
        uint32 leftover_size = EOM - available - block::meta_size;
        leftover_size -= unaligned_bytes(leftover_size, block::alignment);
        if (leftover_size >= block::split_threshold) {
            block * leftover_block = new (available) block(leftover_size, prev_allocated_block);
            // temporarily create block on the right to pass block consistency test
            debug_only(new (leftover_block->right_adjacent()) block(0, leftover_block));
            free_blocks->put_block(leftover_block);
            prev_allocated_block = leftover_block;
            EOM = available + leftover_block->size_with_meta();
        } else {
            EOM = available;
        }
        // right technical border block to prevent block merge overflow
        block * right_border = new (EOM) block(0, prev_allocated_block);
        block::checkout(right_border);
        // remember how much memory left to user
        user_available_memory_size = reinterpret_cast<char *>(right_border) - reinterpret_cast<char *>(left_border) - block::meta_size;
    }


    inline memalloc::block * memalloc::merge_unused(memalloc::block * blk) noexcept {
        // merge left until used block or max_block_size
        while ((blk->size() < const_::max_block_size) && blk->left_adjacent()->is_free()) {
            if (blk->size() + blk->left_adjacent()->size_with_meta() <= const_::max_block_size) {
                block_list::unlink(blk->left_adjacent());
                blk = block::merge(blk->left_adjacent(), blk);
            } else {
                break;
            }
        }
        // merge right until used block or max_block_size
        while ((blk->size() < const_::max_block_size) && blk->right_adjacent()->is_free()) {
            if (blk->size() + blk->right_adjacent()->size_with_meta() <= const_::max_block_size) {
                block_list::unlink(blk->right_adjacent());
                blk = block::merge(blk, blk->right_adjacent());
            } else {
                break;
            }
        }
        return blk;
    }


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
        if (arena_begin < u8_ptr && u8_ptr < arena_end) {
            debug_only(block::from_user_ptr(ptr)->test_check());
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
        block * free_block = free_blocks->try_get_block(nsize);
        if (free_block != nullptr) {
            return block::checkout(free_block);
        }
        // 2. Try to split the biggest available block
        block * big_block = free_blocks->try_get_biggest_block(nsize);
        if (big_block != nullptr) {
            debug_assert(big_block->size() >= nsize);
            block * leftover;
            tie(free_block, leftover) = block::split(big_block, nsize);
            if (leftover) {
                free_blocks->put_block(leftover);
            }
            return block::checkout(free_block);
        }
        return nullptr;
    }


    void * memalloc::try_realloc_inplace(void * ptr, const size_t new_size) noexcept {
        debug_assert(valid_addr(ptr));
        debug_assert(new_size <= const_::allocation_limit);
        debug_assert(new_size > 0);
        block * blk = block::from_user_ptr(ptr);

        if (blk->size() >= new_size) {
            return blk;
        }
        uint32 available_on_the_right = 0;
        uint32 neccassary = new_size - blk->size();
        while (available_on_the_right < neccassary && blk->right_adjacent()->is_free()) {
            available_on_the_right += blk->right_adjacent()->size_with_meta();
            blk = blk->right_adjacent();
        }
        if (available_on_the_right >= neccassary) {
            while (blk->size() < new_size) {
                blk = block::merge(blk, blk->right_adjacent());
            }
            debug_assert(blk->size() >= new_size);
            block * leftover;
            tie(blk, leftover) = block::split(blk, new_size);
            if (leftover != nullptr) {
                free_blocks->put_block(leftover);
            }
            return blk;
        } else {
            // we don't have enough memory to extend block up to `new_size`
            return nullptr;
        }
    }


    void memalloc::free(void * ptr) noexcept {
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        block::unuse(blk);
        // try to merge it
        blk = merge_unused(blk);
        free_blocks->put_block(blk);
    }


    size_t memalloc::_reveal_actual_size(void * ptr) const noexcept {
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        return blk->size();
    }


} // namespace cachelot


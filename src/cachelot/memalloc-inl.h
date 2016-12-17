#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#  error "Header is not for direct use"
#endif

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/**
 * @page   memalloc
 * @brief  memalloc implementation
 *
 * ## Memalloc Internals
 *
 * ### Blocks
 * memalloc splits all provided memory on blocks (aka slabs) of variable size;<br/>
 * All blocks are doubly-linked with adjacents:<br/>
 *  `prev <- left_adjacent_offset [block] size -> next`<br/>
 * Linkage allows to coalesce smaller blocks into bigger<br/>
 *
 * ### Blocks grouping
 * Blocks of the similar size stored in the same *size class*, double-linked circular list of blocks
 * There is a table grouping blocks by their size (@ref free_blocks_by_size), to serve allocations. Each cell of this table is doubly-linked list of blocks of near size.<br/>
 * The easiest way to think about grouping near size blocks  as of the sizes are powers of 2, as follows:<br/>
 *  `[256, 512, 1024, 2048 ... 2^last_power_of_2]`<br/>
 * However, additionally each range [ 2^power .. 2^(power+1) ) split by `num_sub_cells_per_power`, as follows `(num_sub_cells_per_power = 32)`:<br/>
 *  `[256, 264, 272, 280, 288, 296 ... 512, 528, 544, ... allocation_limit]`<br/>
 * The first power of 2 and num_sub_cells_per_power defines alignment of each allocation:<br/>
 *  `alignment = (2^(first_power_of_2) - 2^(first_power_of_2 - 1)) / num_sub_cells_per_power`<br/>
 * Moreover, blocks of size below 2^<sup>(first_power_of_2)</sup> are considered as a small. The range `[0..small_block_boundary)` also split on `num_sub_cells_per_power` cells and stored in the `table[0]`.<br/>
 * Finally, table looks as follows:<br/>
 *  `[0, 8, 16, 24 ... 248, 256, 264, ... 2^last_power_of_2, 2^last_power_of_2 + cell_difference ...]`<br/>
 *
 *  ### Block split / merge and border blocks
 *
 *<br/>
 */


namespace cachelot {

// TODO: Use bit in left_adjacent_offset as "is left block free" marker
// This may speedup many lookups


////////////////////////////////// pages //////////////////////////////////////

    /**
     * Whole amount of memory is logicaly split by fixed-size pages
     * When out of memory, some page may be evicted to free space for the newly insterted item(s)
     */
    class memalloc::pages {
    public:
        struct page_boundaries {
            const uint8 * const begin;
            const uint8 * const end;
        };

        struct page_info {
            // all pages are lined up by last access time
            intrusive_list_node lru_link;
            uint64 num_hits = 0;
            uint64 num_evictions = 0;
        };
    public:
        /// Size of the page
        const size_t page_size;
        /// Total number of pages in arena
        const size_t num_pages;
        /// Pointer to the arena begin
        uint8 * const arena_begin;
        /// Pointer to the arena end
        uint8 * const arena_end;

        /// constructor
        pages(const size_t the_page_size, uint8 * const the_arena_begin, uint8 * const the_arena_end)
            : page_size(the_page_size)
            , num_pages((the_arena_end - the_arena_begin) / the_page_size)
            , arena_begin(the_arena_begin)
            , arena_end(the_arena_end)
            , log2_page_size(log2u(the_page_size))
            , all_pages(num_pages) {
            debug_assert(page_size > 0); debug_assert(ispow2(page_size));
            debug_assert(num_pages >= 4); debug_assert(ispow2(num_pages));
            // base_addr must be properly aligned
            debug_assert(reinterpret_cast<size_t>(arena_begin) % page_size == 0);
            // memory amount must be divisible by the page size
            debug_assert((arena_end - arena_begin) % the_page_size == 0);
            // place all pages in the LRU list
            for (size_t page_no = 0; page_no < num_pages; ++page_no) {
                lru_pages.push_front(&all_pages[page_no]);
            }
        }

        /// retrieve metadata of the page containing address specified
        page_info * page_info_from_addr(const void * const ptr) noexcept {
            const auto page_no = page_no_from_addr(ptr);
            return &all_pages[page_no];
        }

        /// retrieve boundaries of the page containing address specified
        tuple<const uint8 * const, const uint8 * const> page_boundaries_from_addr(const void * const ptr) const noexcept {
            const auto page_no = page_no_from_addr(ptr);
            const uint8 * const page_begin = arena_begin + (page_no * page_size);
            const uint8 * const page_end = page_begin + page_size;
            return make_tuple(page_begin, page_end);
        }

        /// increase chances to survive eviction for the page containing address specified
        void touch(const void * const ptr) noexcept {
            const auto page = page_info_from_addr(ptr);
            page->num_hits += 1;
            // move closer to front
            lru_pages.move_front(page);
        }

        /// retrieve the best candidate for eviction and reuse
        tuple<uint8 *, uint8 *> page_to_reuse() noexcept {
            page_info * least_used = lru_pages.back();
            least_used->num_evictions += 1;
            // make it first to prolong its life
            lru_pages.remove(least_used);
            lru_pages.push_front(least_used);
            // get
            for (size_t page_no = 0; page_no < num_pages; ++page_no) {
                if (least_used == &all_pages[page_no]) {
                    uint8 * page_begin = arena_begin + (page_no * page_size);
                    uint8 * page_end = page_begin + page_size;
                    return make_tuple(page_begin, page_end);
                }
            }
            debug_assert(false);
            return tuple<uint8 *, uint8 *>(nullptr, nullptr);
        }

        /// check that address is within arena range
        bool valid_addr(const void * const ptr) const noexcept {
            auto u8_ptr = reinterpret_cast<const uint8 * const>(ptr);
            return u8_ptr >= arena_begin && u8_ptr < arena_end;
        }
    private:
        unsigned page_no_from_addr(const void * const ptr) const noexcept {
            debug_assert(valid_addr(ptr));
            auto ui8_ptr = reinterpret_cast<const uint8 * const>(ptr);
            auto page_no = static_cast<unsigned>(ui8_ptr - arena_begin) >> log2_page_size;
            debug_assert(page_no < num_pages);
            return page_no;
        }

    private:
        const size_t log2_page_size;
        std::vector<page_info> all_pages;
        intrusive_list<page_info, &page_info::lru_link> lru_pages;
    private:
        friend struct test_memalloc::test_pages;
    };



////////////////////////////////// block //////////////////////////////////////


    /// single allocation chunk with metadata
    class memalloc::block {
        struct {
            uint32 size : 31;   /// amount of memory available to user
            bool used : 1;      /// indicate whether block is used
            uint32 left_adjacent_offset;  /// offset of previous block in continuous arena
            /// debug marker to identify corrupted memory
            debug_only(uint32 dbg_marker1;)
            debug_only(uint32 dbg_marker2;)
        } meta;

        union {
            /// link for circular list of blocks in free_blocks_by_size table
            intrusive_list_node group_by_size_link;
            /// pointer to actual memory given to the user
            uint8 memory_[1];
        };

    public:
        /// allocation alignment
        constexpr static uint32 alignment = alignof(void *);
        /// size of block metadata
        constexpr static uint32 header_size = sizeof(meta);
        /// minimal amount of memory available to the user
        constexpr static uint32 min_memory = 64 - header_size;
        /// impossible to split block if less than `split_threshold` memory would left
        constexpr static uint32 split_threshold = header_size + min_memory;

        /// constructor
        explicit block(const uint32 the_size, const uint32 left_adjacent_block_offset) noexcept {
            static_assert(alignment >= alignof(decltype(block::meta)) &&
                          unaligned_bytes(alignment, alignof(decltype(block::meta))) == 0,
                          "Block metadata must be properly aligned");
            static_assert(ispow2(alignment), "alignment must be a power of 2");
            static_assert(unaligned_bytes(min_memory, alignment) == 0, "Minimal allocation size is not properly aligned");
            // User memory must be properly aligned
            debug_assert(unaligned_bytes(memory_, alignment) == 0);
            // set debug marker
            debug_only(meta.dbg_marker1 = DBG_MARKER1_INIT);
            debug_only(meta.dbg_marker2 = DBG_MARKER2_INIT);
            meta.size = the_size;
            meta.used = false;
            meta.left_adjacent_offset = left_adjacent_block_offset;
        }

        ~block(); // Must not be called

        /// amount of memory available to user
        uint32 size() const noexcept { return meta.size; }

        /// amount of memory including metadata
        uint32 size_with_header() const noexcept { return size() + header_size; }

        /// modify size stored in block metadata
        void set_size(const uint32 new_size) noexcept { meta.size = new_size; }

        /// check whether block is used
        bool is_used() const noexcept { return meta.used == true; }

        /// check whether block is free
        bool is_free() const noexcept { return meta.used == false; }

        /// mark block as used
        void set_used() noexcept { debug_assert(meta.used == false); meta.used = true; }

        /// mark block as free
        void set_free() noexcept { debug_assert(meta.used == true); meta.used = false; }

        /// return pointer to memory available to user
        void * memory() noexcept { return memory_; }

        /// block adjacent to the left in arena
        block * left_adjacent() const noexcept {
            debug_assert(meta.left_adjacent_offset > 0);
            const uint8 * this_ = reinterpret_cast<const uint8 *>(this);
            const block * left = reinterpret_cast<const block *>(this_ - meta.left_adjacent_offset);
            return const_cast<block *>(left);
        }

        /// block adjacent to the right in arena
        block * right_adjacent() const noexcept {
            auto this_ = reinterpret_cast<const uint8 *>(this);
            const block * right = reinterpret_cast<const block *>(this_ + size_with_header());
            return const_cast<block *>(right);
        }

        /// get block structure back from pointer given to user
        static block * from_user_ptr(void * ptr) noexcept {
            uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
            memalloc::block * result = reinterpret_cast<memalloc::block *>(u8_ptr - memalloc::block::header_size);
            debug_only(result->assert_dbg_marker());
            return result;
        }

        /// split given block `blk` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<block *, block *> split(const std::unique_ptr<pages> & pgs, block * blk, uint32 new_size) noexcept {
            debug_only(blk->__debug_sanity_check(pgs));
            debug_assert(blk->is_free());

            // roundup
            if (new_size > min_memory) {
                new_size += unaligned_bytes(blk->memory_ + new_size, alignment);
            } else {
                new_size = min_memory;
            }
            memalloc::block * leftover = nullptr;
            if (new_size < blk->size() && blk->size() - new_size >= split_threshold) {
                const uint32 old_size = blk->size();
                memalloc::block * block_after_next = blk->right_adjacent();
                blk->set_size(new_size);
                debug_assert(old_size - new_size > block::header_size + block::alignment);
                leftover = new (blk->right_adjacent()) memalloc::block(old_size - new_size - header_size, new_size + header_size);
                if (reinterpret_cast<uint8 *>(block_after_next) < pgs->arena_end) {
                    block_after_next->meta.left_adjacent_offset = leftover->size_with_header();
                }
            }
            debug_only(blk->__debug_sanity_check(pgs));
            debug_only(if (leftover) { leftover->__debug_sanity_check(pgs); });
            return make_tuple(blk, leftover);
        }

        /// merge two blocks up to `max_allocation`
        static block * merge(const std::unique_ptr<pages> & pgs, block * left_block, block * right_block) noexcept {
            // Sanity check
            debug_assert(right_block->left_adjacent() == left_block);
            debug_assert(left_block->right_adjacent() == right_block);
            debug_only(left_block->__debug_sanity_check(pgs));
            debug_only(right_block->__debug_sanity_check(pgs));
            // Only free blocks may be merged, user data must left untouched
            debug_assert(left_block->is_free()); debug_assert(right_block->is_free());
            memalloc::block * block_after_right = right_block->right_adjacent();
            left_block->set_size(left_block->size() + right_block->size_with_header());
            if (reinterpret_cast<uint8 *>(block_after_right) < pgs->arena_end) {
                block_after_right->meta.left_adjacent_offset = left_block->size_with_header();
            }
            debug_only(left_block->__debug_sanity_check(pgs));
            return left_block;
        }

        // check that marker signature is untouched
        void assert_dbg_marker() const {
            debug_assert(meta.dbg_marker1 == DBG_MARKER1_INIT);
            debug_assert(meta.dbg_marker2 == DBG_MARKER2_INIT);
        }

        /// check that block doesn't cross page boundaries and also check that neighbours are connected correctly
        void __debug_sanity_check(const std::unique_ptr<pages> & pg, bool check_neighbours = true) const {
            debug_only(assert_dbg_marker());
            const uint8 * page_begin; const uint8 * page_end;
            tie(page_begin, page_end) = pg->page_boundaries_from_addr(this);
            auto this_ = reinterpret_cast<const uint8 *>(this);
            debug_assert(this_ >= page_begin);
            debug_assert(this_ + size_with_header() <= page_end);
            if (not check_neighbours) {
                return;
            }
            if (this_ > page_begin) {
                const auto left = left_adjacent();
                debug_only(left->assert_dbg_marker());
                debug_assert(left->right_adjacent() == this);
                debug_assert(reinterpret_cast<const uint8 *>(left) >= page_begin);
                (void)left;
            }
            if (this_ + this->size_with_header() < page_end) {
                const auto right = right_adjacent();
                debug_only(right->assert_dbg_marker());
                debug_assert(right->left_adjacent() == this);
                debug_assert(reinterpret_cast<const uint8 *>(right) + right->size_with_header() <= page_end);
                (void)right;
            }
            (void)pg; (void)page_begin; (void)page_end; (void)this_;
        }

    private:
        debug_only(static constexpr uint32 DBG_MARKER1_INIT = 1234567890;)
        debug_only(static constexpr uint32 DBG_MARKER2_INIT = 987654321;)

        // Allows to create fake blocks in the unit tests
        friend struct test_memalloc::test_free_blocks_by_size;
        // TODO: Better solution?
        friend class memalloc;
    };


////////////////////////////////// free_blocks_by_size ///////////////////////////

    /**
     * @section free_blocks_by_size Segregation
     * Table of block_list where each cell represent certain range of sizes (size class)
     *
     * It is organized as a continuous array where size_classs are lists of blocks within a size class.
     * There are two bit indexes intended to minimize cache misses and to speed up search of block of maximal size.
     * In each bitmask: set bit indicates that there is *probably* block of corresponding size
     * unset bit indicates that there is *definitely* no blocks of corresponding size
     *  - Each bit in first level index `pow_index` refers group of size classes of corresponding power of 2
     * @code
     *  // depends on first_power_of_2_offset
     *  0000 0001 => [32..64)
     *  0000 0010 => [64..128)
     *  // ... and so on until last_power_of_2
     * @endcode
     *  - Each bit of second level index refers to the corresponding size class (sub-block in power of 2)
     * @code
     *  [0000 0000][0000 0001] => [32..36) // zero sub-block of first_power_of_2_offset
     *  [0000 0000][0000 0010] => [36..40) // 1st sub-block of first_power_of_2_offset
     *  [0000 0000][0000 0100] => [40..44) // 2nd sub-block
     *  // ...
     *  [0000 0001][0000 0000] => [64..72) // zero sub-block of next power of 2
     *  // and so on until max_allocation
     * @endcode
     */
    class memalloc::free_blocks_by_size {
        // list of the same class size free blocks
        typedef intrusive_list<memalloc::block, &memalloc::block::group_by_size_link> size_class_list;
        //
        // ! Important: the following constants affect alignment, min allocation size and more
        // Handle with care

        // number of second level cells per first level cell
        // (first level cells are powers of 2 in range `zero-cell,[first_power_of_2..last_power_of_2)`)
        constexpr static uint32 num_sub_cells_per_power = 32; // bigger value means less memory overhead but increases table size
                                                              // affects alignment and min/max allocation sizes
        // First (index 1) power of 2 in block table
        // Block alignment depends on this value:
        //
        // small_block_boundary = 2 ^ first_power_of_2
        // min_block_diff = small_block_boundary / num_sub_cells_per_power
        // min_block_diff must be greater or equal to alignment
        //
        #if (CACHELOT_PLATFORM_BITS == 32)
        static constexpr uint32 first_power_of_2 = 7;  // small block is 128 bytes, aligmnet = 4
        #elif (CACHELOT_PLATFORM_BITS == 64)
        static constexpr uint32 first_power_of_2 = 8;  // small block is 256 bytes, aligmnet = 8
        #endif
        // All sizes below this power of 2 go to the small block cell (zero cell)
        static constexpr uint32 small_block_boundary = pow2(first_power_of_2);
        // Minimal difference in size among blocks
        static constexpr uint32 min_block_diff = small_block_boundary / num_sub_cells_per_power;
        // Last power of 2, all blocks are below this power
        const uint32 last_power_of_2;
        // Number of powers of 2 to serve
        const uint32 num_powers_of_2_plus_small_blocks_cell;
        // Allocation limit depends on the page size
        const uint32 page_size;
    private:
        /**
         * position in the table
         *
         * position is split on pow2 index and subblock index to cooperate with the bitmask index
         * `pow_index` means power of 2 offset from the `first_power_of_2` counting zero cell
         * `sub_index` additionally divides each power of 2 by `num_sub_cells_per_power`
         */
        struct position {
            /// corresponding power of 2
            uint32 pow_index;
            /// index of sub-block within current [pow_index .. next_power_of_2) range
            uint32 sub_index;

            /// private constructor used by free_blocks_by_size
            explicit constexpr position(const uint32 power_index, const uint32 sub_block_index) noexcept
                : pow_index(power_index), sub_index(sub_block_index) {
            }

            constexpr position()
                : pow_index(std::numeric_limits<uint32>::max()), sub_index(std::numeric_limits<uint32>::max()) {}

            /// calculate index of target list in the array
            constexpr uint32 absolute() const noexcept {
                return pow_index * num_sub_cells_per_power + sub_index;
            }

            constexpr position(const position &) noexcept = default;
            position & operator= (const position &) noexcept = default;
        };

        /// calculate size class by `requested_size` while inserting block
        position position_from_size(const uint32 requested_size) const noexcept {
            debug_assert(requested_size <= (page_size - block::header_size));
            if (requested_size >= small_block_boundary) {
                uint32 power2_idx = log2u(requested_size);
                debug_assert(power2_idx >= first_power_of_2);
                uint32 sub_block_idx = (requested_size >> (power2_idx - log2u(num_sub_cells_per_power))) - num_sub_cells_per_power;
                debug_assert(sub_block_idx < num_sub_cells_per_power);
                power2_idx = power2_idx - first_power_of_2 + 1;
                debug_assert(power2_idx > 0); debug_assert(power2_idx < last_power_of_2);
                return position(power2_idx, sub_block_idx);
            } else {
                // small block
                return position(0, requested_size >> log2u(min_block_diff));
            }
        }

        /// check if table cell at `pos` is non empty according to the bit index
        /// @return `true` cell is marked as non empty, `false` otherwise
        bool bit_index_probe(position pos) const noexcept {
            return bit::isset(first_level_bit_index, pos.pow_index) && bit::isset(second_level_bit_index[pos.pow_index], pos.sub_index);
        }

        /// mark cell at `pos` as empty in bit index
        void bit_index_mark_empty(position pos) noexcept {
            // 1. update second level index
            second_level_bit_index[pos.pow_index] = bit::unset(second_level_bit_index[pos.pow_index], pos.sub_index);
            // 2. update first level index (only if all corresponding powers of 2 are empty)
            if (second_level_bit_index[pos.pow_index] == 0) {
                first_level_bit_index = bit::unset(first_level_bit_index, pos.pow_index);
            }
        }

        /// mark cell at `pos` as non empty in bit index
        void bit_index_mark_non_empty(position pos) noexcept {
            second_level_bit_index[pos.pow_index] = bit::set(second_level_bit_index[pos.pow_index], pos.sub_index);
            first_level_bit_index = bit::set(first_level_bit_index, pos.pow_index);
        }


        /// determine next non-empty position from `current`
        tuple<bool, position> next_non_empty(position current) const noexcept {
            auto set_all_bits_after = [=](uint32 bitno) {
                return ~ (pow2(bitno + 1) - 1);
            };

            // try to advance within the same power of 2
            if (current.sub_index < (num_sub_cells_per_power - 1)) {
                const uint32 next_sub_index_mask = (second_level_bit_index[current.pow_index] & set_all_bits_after(current.sub_index));
                if (next_sub_index_mask > 0) {
                    return tuple<bool, position>(true, position(current.pow_index, bit::least_significant(next_sub_index_mask)));
                }
            }
            // try greater power of 2
            if (current.pow_index < (num_powers_of_2_plus_small_blocks_cell - 1)) {
                const uint32 next_pow2_mask = (first_level_bit_index & set_all_bits_after(current.pow_index));
                if (next_pow2_mask > 0) {
                    const uint32 next_pow2 = bit::least_significant(next_pow2_mask);
                    debug_assert(second_level_bit_index[next_pow2] > 0);
                    return tuple<bool, position>(true, position(next_pow2, bit::least_significant(second_level_bit_index[next_pow2])));
                }
            }
            return tuple<bool, position>(false, position());
        }

    public:
        /// constructor
        free_blocks_by_size(const uint32 the_page_size)
            : last_power_of_2(log2u(the_page_size))
            , num_powers_of_2_plus_small_blocks_cell(last_power_of_2 - first_power_of_2 + 1)
            , page_size(the_page_size)
            , first_level_bit_index(0)
            , second_level_bit_index(num_powers_of_2_plus_small_blocks_cell, 0)
            , size_classes_table(num_powers_of_2_plus_small_blocks_cell * num_sub_cells_per_power) {
            static_assert(min_block_diff >= block::alignment,
                        "Minimal block differense is too small for proper block alignment");
            static_assert(num_sub_cells_per_power <= sizeof(decltype(second_level_bit_index)::value_type) * 8,
                        "Not enough bits in second_level_bit_index to reflect num_sub_cells_per_power");
        }

        /// try to get block of the requested `size`
        /// @return pointer to the block or `nullptr` if none was found
        block * try_get_block(const uint32 size) noexcept {
            position pos = position_from_size(size);
            bool has_bigger_block;
            uint32 attempt = 1;
            do {
                // access indexes first, unset bits guarantee that corresponding size_class is empty
                if (bit_index_probe(pos)) {
                    block * blk = nullptr;
                    auto & size_class = size_classes_table[pos.absolute()];
                    if (not size_class.empty()) {
                        debug_assert(attempt == 1 || size_class.front()->size() >= size);
                        if (size_class.front()->size() >= size) {
                            blk = size_class.pop_front();
                        }
                    }
                    if (size_class.empty()) { // Maybe we've just took the last element
                        bit_index_mark_empty(pos);
                    }
                    if (blk != nullptr) {
                        debug_assert(blk->size() >= size);
                        if (attempt == 1) {
                            STAT_INCR(mem.num_free_table_hits, 1);
                        } else {
                            STAT_INCR(mem.num_free_table_weak_hits, 1);
                        }
                        return blk;
                    }
                } else {
                    debug_assert(size_classes_table[pos.absolute()].empty());
                }
                // lookup for block of bigger size
                tie(has_bigger_block, pos) = next_non_empty(pos);
                attempt += 1;
                debug_assert(attempt < num_powers_of_2_plus_small_blocks_cell * num_sub_cells_per_power);
            } while (has_bigger_block);
            // There are no free blocks at least of requested size
            return nullptr;
        }

        /// store block `blk` at position corresponding to its size
        void put_block(block * blk) {
            debug_assert(blk->is_free());
            debug_assert(blk->size_with_header() <= page_size);
            position pos = position_from_size(blk->size());
            auto & size_class = size_classes_table[pos.absolute()];
            size_class.push_front(blk);
            // unconditionally update bit index
            bit_index_mark_non_empty(pos);
        }

        /// remove block `blk` from the free blocks
        void remove_block(block * blk) noexcept {
            size_class_list::unlink(blk);
        }

        /// size of the page, also size of the biggest possible allocation
        uint32 get_page_size() const noexcept {
            // suppress warning about unused private var in Release build
            return page_size;
        }

    private:
        // bit indexes here to speed-up block lookups
        // '1' means maybe there is a block; '0' - definitely there is no blocks
        // Each bit in `first_level_bit_index` reflects power of 2 with all its sub-cells, so it masks `second_level_bit_index` too
        uint32 first_level_bit_index;
        // Second level bit index reflects each cell in the size_classes_table
        std::vector<uint32> second_level_bit_index;
        // Main block table. Each cell is a doubly-linked list of "same size class" blocks
        std::vector<size_class_list> size_classes_table;
    private:
        // Unit tests
        friend struct test_memalloc::test_free_blocks_by_size;
    };

////////////////////////////////// memalloc //////////////////////////////////////

    inline memalloc::memalloc(const size_t memory_limit, const uint32 the_page_size)
        : arena_size(memory_limit)
        , page_size(the_page_size)
        , m_arena(nullptr, &std::free) {
        debug_assert(ispow2(memory_limit));
        debug_assert(page_size > 0);
        debug_assert(ispow2(page_size));
        debug_assert(memory_limit >= (page_size * 4));
        debug_assert(memory_limit % page_size == 0);
        STAT_SET(mem.limit_maxbytes, memory_limit);
        STAT_SET(mem.page_size, page_size);
        m_arena.reset(aligned_alloc(page_size, arena_size));
        if (m_arena == nullptr) {
            throw std::bad_alloc();
        }
        auto arena_begin = reinterpret_cast<uint8 *>(m_arena.get());
        m_pages.reset(new pages(page_size, arena_begin, arena_begin + memory_limit));
        m_free_blocks.reset(new free_blocks_by_size(page_size));
        // pointer to currently non-used memory
        uint8 * available = arena_begin;
        // End-Of-Memory marker
        uint8 * EOM = available + arena_size;

        // split all arena on blocks and store them in free blocks table

        uint32 left_adjacent_block_offset = 0;
        while (available < EOM) {
            debug_assert(static_cast<size_t>(EOM - available) >= page_size);
            block * huge_block = new (available) block(page_size - block::header_size, left_adjacent_block_offset);
            // store block at max pos of table
            m_free_blocks->put_block(huge_block);
            available += page_size;
            left_adjacent_block_offset = page_size;
        }
        debug_assert(available == EOM);
        #if defined(ADDRESS_SANITIZER)
        m_arena.reset(nullptr);
        #endif
    }

    inline memalloc::block * memalloc::merge_free_left(memalloc::block * blk) noexcept {
        // Note: we may not have 'left_adjacent' here.
        // We have to check that offset is within the page boundaries first.
        const uint8 * page_begin; const uint8 * page_end;
        tie(page_begin, page_end) = m_pages->page_boundaries_from_addr(blk);

        // merge to the left
        const uint8 * left_block_boundary = reinterpret_cast<const uint8 * >(blk);
        while (left_block_boundary > page_begin && blk->left_adjacent()->is_free()) {
            auto left = blk->left_adjacent();
            debug_assert(left->size_with_header() + blk->size_with_header() <= page_size);
            m_free_blocks->remove_block(left);
            // Sanity check
            blk = block::merge(m_pages, left, blk);
            left_block_boundary = reinterpret_cast<const uint8 * >(blk);
            // ensure we don't cross
            debug_assert(left_block_boundary >= page_begin);
        }
        return blk;
    }

    inline memalloc::block * memalloc::merge_free_right(memalloc::block * blk) noexcept {
        // Note: we may not have 'right_adjacent' here.
        // We have to check that offset is within the page boundaries first.
        const uint8 * page_begin; const uint8 * page_end;
        tie(page_begin, page_end) = m_pages->page_boundaries_from_addr(blk);

        // merge to the right
        const uint8 * right_block_boundary = reinterpret_cast<const uint8 * >(blk) + blk->size_with_header();
        while (right_block_boundary < page_end && blk->right_adjacent()->is_free()) {
            auto right = blk->right_adjacent();
            debug_assert(blk->size_with_header() + right->size_with_header() <= page_size);
            m_free_blocks->remove_block(right);
            // Sanity check
            blk = block::merge(m_pages, blk, right);
            right_block_boundary = reinterpret_cast<const uint8 * >(blk) + blk->size_with_header();
            // ensure we don't cross
            debug_assert(right_block_boundary <= page_end);
        }
        return blk;
    }

    inline memalloc::block * memalloc::merge_free(memalloc::block * blk) noexcept {
        blk = merge_free_left(blk);
        blk = merge_free_right(blk);
        return blk;
    }


    inline void * memalloc::checkout(block * blk, const uint32 requested_size) noexcept {
        debug_assert(blk->size() >= requested_size);
        block * leftover;
        tie(blk, leftover) = block::split(m_pages, blk, requested_size);
        if (leftover != nullptr) {
            m_free_blocks->put_block(leftover);
        }
        blk->set_used();
        STAT_INCR(mem.used_memory, blk->size_with_header());
        return blk->memory();
    }


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        if (m_pages->valid_addr(ptr)) {
            block::from_user_ptr(ptr);
            return true;
        }
        return false;
    }

    inline void memalloc::touch(void * ptr) noexcept {
        #if defined(ADDRESS_SANITIZER)
        return;
        #endif
        debug_assert(valid_addr(ptr));
        // ensure we're touching the used block
        debug_assert(block::from_user_ptr(ptr)->is_used());
        // additional sanity check
        debug_only(block::from_user_ptr(ptr)->__debug_sanity_check(m_pages));
        // touch corresponding page
        m_pages->touch(ptr);
    }


    template <typename ForeachFreed>
    inline void * memalloc::alloc_or_evict(const size_t requested_size, bool evict_if_necessary, ForeachFreed on_free_block) {
        debug_assert(requested_size > 0); debug_assert(requested_size <= page_size);
        const auto size = static_cast<uint32>(requested_size);

        #if defined(ADDRESS_SANITIZER)
        return std::malloc(size);
        #endif

        STAT_INCR(mem.num_malloc, 1);
        STAT_INCR(mem.total_requested, size);
        // 1. Search among the free blocks
        {
            block * found_blk = m_free_blocks->try_get_block(size);
            if (found_blk != nullptr) {
                m_pages->touch(found_blk);
                auto mem = checkout(found_blk, size);
                STAT_INCR(mem.total_served, reveal_actual_size(mem));
                return mem;
            }
        }
        // 2. Try to evict existing block to free some space
        if (evict_if_necessary) {
            uint8 * page_begin, * page_end;
            tie(page_begin, page_end) = m_pages->page_to_reuse();
            // clean the page, evict used blocks, remove free blocks from the free_blocks list
            auto blk = reinterpret_cast<block *>(page_begin); // every page starts with the block
            debug_only(blk->assert_dbg_marker());
            const uint32 left_adjacent_block_offset = blk->meta.left_adjacent_offset;
            do {
                if (blk->is_used()) {
                    // notify user that memory is evicted
                    on_free_block(blk->memory());
                    STAT_INCR(mem.evictions, 1);
                    STAT_DECR(mem.used_memory, blk->size_with_header());
                } else {
                    // remove block from the free blocks list
                    m_free_blocks->remove_block(blk);
                }
                blk = blk->right_adjacent();
            } while (reinterpret_cast<uint8 *>(blk) < page_end);
            debug_assert(reinterpret_cast<uint8 *>(blk) == page_end);

            // adjust the fist block in the next page
            if (page_end < reinterpret_cast<uint8 *>(m_arena.get()) + arena_size) {
                reinterpret_cast<block *>(page_end)->meta.left_adjacent_offset = page_size;
            }
            auto whole_page_block = new (page_begin) block(page_size - block::header_size, left_adjacent_block_offset);
            auto mem = checkout(whole_page_block, size);
            STAT_INCR(mem.total_served, reveal_actual_size(mem));
            return mem;
        }
        // if we reach here ...
        STAT_INCR(mem.num_alloc_errors, 1);
        STAT_INCR(mem.total_unserved, size);
        return nullptr;
    }


    inline void * memalloc::realloc_inplace(void * ptr, const size_t new_size) noexcept {
        #if defined(ADDRESS_SANITIZER)
        return nullptr;
        #endif
        debug_assert(new_size > 0); debug_assert(new_size <= page_size);
        debug_assert(valid_addr(ptr));

        STAT_INCR(mem.num_realloc, 1);
        block * blk = block::from_user_ptr(ptr);
        debug_only(blk->__debug_sanity_check(m_pages));

        blk->set_free();
        STAT_DECR(mem.used_memory, blk->size_with_header());

        const auto size = static_cast<uint32>(new_size);

        // 1. shrink the block
        if (new_size <= blk->size()) {
            return checkout(blk, size);
        }

        // 2. try to expand the block to the right
        const auto old_block_size = blk->size();
        STAT_INCR(mem.total_realloc_requested, new_size - old_block_size);
        blk = merge_free_right(blk);
        if (blk->size() >= new_size) {
            auto mem = checkout(blk, size);
            STAT_INCR(mem.total_realloc_served, reveal_actual_size(mem) - old_block_size - block::header_size);
            return mem;
        } else {
            // give up
            STAT_INCR(mem.total_realloc_unserved, new_size - old_block_size);
            // return block to its original state
            checkout(blk, old_block_size);
            STAT_INCR(mem.num_realloc_errors, 1);
            return nullptr;
        }
    }


    inline void memalloc::free(void * ptr) noexcept {

        #if defined(ADDRESS_SANITIZER)
        std::free(ptr); return;
        #endif

        debug_assert(valid_addr(ptr));
        STAT_INCR(mem.num_free, 1);
        block * blk = block::from_user_ptr(ptr);
        debug_only(blk->__debug_sanity_check(m_pages));
        blk->set_free();
        STAT_DECR(mem.used_memory, blk->size_with_header());
        // merge with neighbours
        blk = merge_free(blk);
        // store for reuse
        m_free_blocks->put_block(blk);
    }


    inline size_t memalloc::reveal_actual_size(void * ptr) const noexcept {
        #if defined(ADDRESS_SANITIZER)
        return 0;
        #endif
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        debug_assert(blk->is_used());
        debug_only(block::from_user_ptr(ptr)->__debug_sanity_check(m_pages));
        return blk->size_with_header();
    }

    inline size_t memalloc::header_size() noexcept {
        return block::header_size;
    }

} // namespace cachelot


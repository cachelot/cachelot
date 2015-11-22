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

// TODO: Make page_size configurable option

// TODO: Use bit in left_adjacent_offset as "is left block free" marker
// This may speedup many lookups


    namespace bi = boost::intrusive;


////////////////////////////////// pages //////////////////////////////////////


    /// Whole amount of memory is logicaly split by fixed-size pages
    /// When out of memory, some page may be evicted to free space for the newly insterted item(s)
    struct memalloc::page_info {
        uint64 num_hits = 0;
        uint64 num_evictions = 0;
        // all pages are lined up by last access time
        typedef bi::list_member_hook<bi::link_mode<bi::auto_unlink>> lru_link_type;
        lru_link_type lru_link;
    };


    /// Fixed-size memory page that can be evicted when out of memory
    class memalloc::page_manager {
    public:
        const uint8 * arena_begin;
        const uint8 * arena_end;
        const size_t page_size;
        const size_t num_pages;
        page_manager(const uint8 * base_addr, const size_t the_page_size, const size_t the_num_pages)
            : arena_begin(base_addr)
            , arena_end(base_addr + the_page_size * the_num_pages)
            , page_size(the_page_size)
            , num_pages(the_num_pages)
            , all_pages(new page_info[the_num_pages]) {
            debug_assert(ispow2(page_size));
            debug_assert(ispow2(num_pages));
            // base_addr must be properly aligned
            debug_assert(unaligned_bytes(base_addr, page_size) == 0);
            for (size_t page_no = 0; page_no < num_pages; ++page_no) {
                lru_pages.push_back(all_pages[page_no]);
            }
        }

        page_info * page_info_from_addr(const void * const ptr) noexcept {
            auto ui8_ptr = reinterpret_cast<const uint8 * const>(ptr);
            debug_assert(ui8_ptr >= arena_begin && ui8_ptr <= arena_end);
            const unsigned page_no = (ui8_ptr - arena_begin) >> log2u(page_size);
            // page_no = (ui8_ptr - arena_begin) / page_size;
            return &all_pages[page_no];
        }

        void touch(page_info * page) noexcept {
            page->num_hits += 1;
            page->lru_link.unlink();
            lru_pages.push_front(*page);
        }

    private:
        std::unique_ptr<page_info[]> all_pages;
        typedef bi::member_hook<page_info, page_info::lru_link_type, &page_info::lru_link> page_member_link_type;
        bi::list<page_info, page_member_link_type, bi::constant_time_size<false>> lru_pages;
    };



////////////////////////////////// block //////////////////////////////////////


    /// single allocation chunk with metadata
    class memalloc::block {
        debug_only(static constexpr uint64 DBG_MARKER = 1234567890987654321;)
        debug_only(static constexpr char DBG_FILLER = 'X';)

        struct {
            uint32 size : 31;   /// amount of memory available to user
            bool used : 1;      /// indicate whether block is used
            uint32 left_adjacent_offset;  /// offset of previous block in continuous arena
            debug_only(uint64 dbg_marker;) /// debug constant marker to identify invalid blocks
        } meta;

        typedef bi::list_member_hook<bi::link_mode<bi::auto_unlink>> _hook_type;
        /// link for circular list of blocks in free_blocks_by_size table
        _hook_type group_by_size_link;
        union {

            /// pointer to actual memory given to the user
            uint8 memory_[1];
        };

    public:
        /// allocation alignment
        constexpr static uint32 alignment = alignof(decltype(block::meta));
        /// size of block metadata
        constexpr static uint32 header_size = sizeof(meta) + sizeof(group_by_size_link);
        /// minimal amount of memory available to the user
        constexpr static uint32 min_memory = 64 - header_size;
        /// impossible to split block if less than `split_threshold` memory would left
        constexpr static uint32 split_threshold = header_size + min_memory;
        /// link in the cell of same size class blocks
        typedef bi::member_hook<memalloc::block, _hook_type, &memalloc::block::group_by_size_link> group_by_size_link_type;

        /// @name constructors
        ///@{

        /// constructor (used by technical 'border' blocks)
        block() noexcept
            : group_by_size_link() {
            meta.used = false;
            meta.size = 0;
            meta.left_adjacent_offset = 0;
            debug_only(meta.dbg_marker = DBG_MARKER);
            static_assert(alignment >= alignof(void *), "alignment must be greater or equal to the pointer alignment");
            static_assert(ispow2(alignment), "alignment must be a power of 2");
            static_assert(unaligned_bytes(min_memory, alignment) == 0, "Minimal allocation size is not properly aligned");
        }

        /// constructor used for normal blocks
        explicit block(const uint32 the_size, const block * left_adjacent_block) noexcept
            : group_by_size_link() {
            // User memory must be properly aligned
            debug_assert(unaligned_bytes(memory_, alignment) == 0);
            // set debug marker
            debug_only(meta.dbg_marker = DBG_MARKER);
            // Fill-in memory with debug pattern
            debug_only(std::memset(memory_, DBG_FILLER, the_size));
            meta.size = the_size;
            meta.used = false;
            meta.left_adjacent_offset = left_adjacent_block->size_with_header();
        }
        ///@}

        ~block(); // Must not be called

        /// amount of memory available to user
        uint32 size() const noexcept { return meta.size; }

        /// amount of memory including metadata
        uint32 size_with_header() const noexcept { return size() + header_size; }

        /// modify size stored in block metadata
        void set_size(const uint32 new_size) noexcept { meta.size = new_size; }

        /// check whether block is used
        bool is_used () const noexcept { return meta.used; }

        /// check whether block is free
        bool is_free () const noexcept { return not meta.used; }

        /// check whether block is left border block
        bool is_border() const noexcept { return meta.size == 0; }

        /// check whether block is left border block
        bool is_left_border() const noexcept { return is_border() && meta.left_adjacent_offset == 0; }

        /// check whether block is right border block
        bool is_right_border() const noexcept { return is_border() && meta.left_adjacent_offset > 0; }

        /// block adjacent to the left in arena
        block * left_adjacent() const noexcept {
            debug_assert(not is_left_border());
            const uint8 * this_ = reinterpret_cast<const uint8 *>(this);
            const block * left = reinterpret_cast<const block *>(this_ - meta.left_adjacent_offset);
            return const_cast<block *>(left);
        }

        /// check whether block is linked into freelist
        bool is_linked() const noexcept { return group_by_size_link.is_linked(); }

        /// remove this block from the circular list of the free blocks
        void unlink() noexcept {
            debug_assert(is_linked());
            group_by_size_link.unlink();
        }

        /// block adjacent to the right in arena
        block * right_adjacent() const noexcept {
            // this must be either non-technical or left border block
            debug_assert(not is_right_border());
            auto this_ = reinterpret_cast<const uint8 *>(this);
            const block * right = reinterpret_cast<const block *>(this_ + size_with_header());
            return const_cast<block *>(right);
        }

        /// return pointer to memory available to user
        void * memory() noexcept {
            debug_assert(not is_border());
            return memory_;
        }

        /// debug only block health check (against memory corruptions)
        void __dbg_sanity_check() const noexcept {
                            // check self
            debug_assert(   meta.dbg_marker == DBG_MARKER                                                           );
            debug_assert(   meta.size >= min_memory || is_border()                                                  );
            debug_only(     const auto this_ = reinterpret_cast<const uint8 *>(this)                                );
                            // check left
            debug_only(     if (not is_left_border()) {                                                             );
            debug_only(         auto left = reinterpret_cast<const block *>(this_ - meta.left_adjacent_offset)      );
            debug_assert(       left->meta.dbg_marker == DBG_MARKER                                                 );
            debug_assert(       reinterpret_cast<const uint8 *>(left) + left->size_with_header() == this_           );
            debug_only(     }                                                                                       );
                            // check right
            debug_only(     if (not is_right_border()) {                                                            );
            debug_only(         auto right = reinterpret_cast<const block *>(this_ + size_with_header())            );
            debug_assert(       right->meta.dbg_marker == DBG_MARKER                                                );
            debug_assert(       reinterpret_cast<const uint8 *>(right) - right->meta.left_adjacent_offset == this_  );
            debug_only(     }                                                                                       );
        }

        /// get block structure back from pointer given to user
        static block * from_user_ptr(void * ptr) noexcept {
            uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
            memalloc::block * result = reinterpret_cast<memalloc::block *>(u8_ptr - offsetof(memalloc::block, memory_));
            debug_only(result->__dbg_sanity_check());
            return result;
        }

        /// mark block as used and return pointer to available memory to user
        static void * checkout(block * blk) noexcept {
            debug_only(blk->__dbg_sanity_check());
            debug_assert(not blk->is_used());
            blk->meta.used = true;
            return blk->memory_;
        }

        /// mark block as free
        static void unuse(block * blk) noexcept {
            debug_only(blk->__dbg_sanity_check());
            // block borders must stay used all the time
            debug_assert(not blk->is_border());
            debug_assert(blk->is_used());
            blk->meta.used = false;
            debug_only(std::memset(blk->memory_, DBG_FILLER, blk->size()));
        }

        /// split given block `blk` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<block *, block *> split(block * blk, uint32 new_size) noexcept {
            debug_only(blk->__dbg_sanity_check());
            debug_assert(blk->is_free());
            debug_assert(not blk->is_border());
            const uint32 new_round_size = new_size > min_memory ? new_size + unaligned_bytes(blk->memory_ + new_size, alignment) : min_memory;
            memalloc::block * leftover = nullptr;
            if (new_round_size < blk->size() && blk->size() - new_round_size > split_threshold) {
                const uint32 old_size = blk->size();
                memalloc::block * block_after_next = blk->right_adjacent();
                blk->set_size(new_round_size);
                debug_assert(old_size - new_round_size > block::header_size + block::alignment);
                // temporary block to pass integrity check
                debug_only(new (reinterpret_cast<uint8 *>(blk) + blk->size_with_header()) block(0, blk));
                leftover = new (blk->right_adjacent()) memalloc::block(old_size - new_round_size - memalloc::block::header_size, blk);
                block_after_next->meta.left_adjacent_offset = leftover->size_with_header();
                debug_only(leftover->__dbg_sanity_check());
            }
            debug_only(blk->__dbg_sanity_check());
            return make_tuple(blk, leftover);
        }

        /// merge two blocks up to `max_allocation`
        static block * merge(block * left_block, block * right_block) noexcept {
            // TODO: Ensure we don't cross the page border
            debug_only(left_block->__dbg_sanity_check());
            debug_only(right_block->__dbg_sanity_check());
            // Border block are there to prevent merge out of arena
            debug_assert(not left_block->is_border()); debug_assert(not right_block->is_border());
            // Sanity check
            debug_assert(right_block->left_adjacent() == left_block);
            debug_assert(left_block->right_adjacent() == right_block);
            // Only free blocks may be merged, user data must left untouched
            debug_assert(left_block->is_free()); debug_assert(right_block->is_free());
            memalloc::block * block_after_right = right_block->right_adjacent();
            left_block->set_size(left_block->size() + right_block->size_with_header());
            block_after_right->meta.left_adjacent_offset = left_block->size_with_header();
            // Erase right block metadata (at this point only left block exists)
            debug_only(std::memset(right_block->memory_, DBG_FILLER, block::header_size));
            return left_block;
        }
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
        typedef bi::list<memalloc::block, memalloc::block::group_by_size_link_type, bi::constant_time_size<false>> size_class_list;
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
        // Allocation limit depends on the page size
        const uint32 allocation_limit;
        // Number of powers of 2 to serve
        const uint32 num_powers_of_2_plus_small_blocks_cell;
    public:
        /**
         * position in the table
         *
         * position is split on pow2 index and subblock index to cooperate with the bitmask index
         * `pow_index` means power of 2 offset from the `first_power_of_2`
         * `sub_index` additionally divides each power of 2 by `num_sub_cells_per_power`
         */
        class position {
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

            /// calculate index of target list in the linear table
            uint32 absolute() const noexcept {
                return pow_index * num_sub_cells_per_power + sub_index;
            }

            friend class free_blocks_by_size;
        public:
            constexpr position(const position &) noexcept = default;
            position & operator= (const position &) noexcept = default;
        };

        /// calculate size class by `requested_size` while inserting block
        position position_from_size(const uint32 requested_size) const noexcept {
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
        constexpr bool bit_index_probe(position pos) const noexcept {
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
        free_blocks_by_size(const size_t the_page_size)
            : last_power_of_2(log2u(the_page_size))
            , allocation_limit(the_page_size - block::header_size)
            , num_powers_of_2_plus_small_blocks_cell(last_power_of_2 - first_power_of_2 + 1)
            , first_level_bit_index(0u)
            , second_level_bit_index(num_powers_of_2_plus_small_blocks_cell, 0u)
            , size_classes_table(num_powers_of_2_plus_small_blocks_cell * num_sub_cells_per_power) {
            static_assert(min_block_diff >= block::alignment,
                        "Minimal block differense is too small for proper block alignment");
            static_assert(num_sub_cells_per_power <= sizeof(decltype(second_level_bit_index)::value_type) * 8,
                        "Not enough bits in second_level_bit_index to reflect num_sub_cells_per_power");
            debug_assert(unaligned_bytes(allocation_limit, block::alignment) == 0);
        }

        /// try to get block of the requested `size`
        /// @return pointer to the block or `nullptr` if none was found
        block * try_get_block(const size_t size) noexcept {
            debug_assert(block::min_memory <= size && size <= allocation_limit);
            debug_assert(unaligned_bytes(size, block::alignment) == 0);
            position pos = position_from_size(size);
            bool has_bigger_block;
            uint32 attempt = 1;
            do {
                // access indexes first, unset bits clearly indicate that corresponding size_class is empty
                if (bit_index_probe(pos)) {
                    block * blk = nullptr;
                    auto & size_class = size_classes_table[pos.absolute()];
                    if (not size_class.empty()) {
                        if (size_class.front().size() >= size) {
                            blk = &size_class.front();
                            size_class.pop_front();
                            debug_only(blk->__dbg_sanity_check());
                        }
                    }
                    if (size_class.empty()) { // Maybe we've just took the last element
                        bit_index_mark_empty(pos);
                    }
                    if (blk != nullptr) {
                        debug_assert(blk->size() >= size);
                        block * leftover;
                        tie(blk, leftover) = block::split(blk, size);
                        if (leftover != nullptr) {
                            put_block(leftover);
                        }
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
            position pos = position_from_size(blk->size());
            auto & size_class = size_classes_table[pos.absolute()];
            size_class.push_front(*blk);
            // unconditionally update bit index
            bit_index_mark_non_empty(pos);
        }


        void test_bit_index_integrity_check() const noexcept {
            for (size_t pow = 0; pow < num_powers_of_2_plus_small_blocks_cell; ++pow) {
                if (second_level_bit_index[pow] > 0) {
                    debug_assert(bit::isset(first_level_bit_index, pow));
                } else {
                    debug_assert(bit::isunset(first_level_bit_index, pow));
                }
                for (unsigned bit_in_subidx = 0; bit_in_subidx < 8; ++bit_in_subidx) {
                    if (bit::isunset(second_level_bit_index[pow], bit_in_subidx)) {
                        debug_assert(size_classes_table[position(pow, bit_in_subidx).absolute()].empty());
                    }
                }
            }
            std::cout << "FLI: [" << std::bitset<32>(first_level_bit_index) << "]" << std::endl;
            for (unsigned sl = 0; sl < second_level_bit_index.size(); ++sl) {
                if (second_level_bit_index[sl] > 0) {
                    std::cout << "SLI" << sl << ": [" << std::bitset<32>(second_level_bit_index[sl]) << "]" << std::endl;
                }
            }
            std::cout << std::endl;
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
    };

////////////////////////////////// memalloc //////////////////////////////////////

    inline memalloc::memalloc(const size_t memory_limit, const size_t the_page_size)
        : arena_size(memory_limit)
        , page_size(the_page_size)
        , arena(aligned_alloc(page_size, arena_size), &aligned_free) {
        STAT_SET(mem.limit_maxbytes, memory_limit);
        STAT_SET(mem.page_size, page_size);
        debug_assert(ispow2(memory_limit));
        debug_assert(ispow2(page_size));
        debug_assert(memory_limit >= (page_size * 4));
        debug_assert(memory_limit % page_size == 0);
        if (arena == nullptr) {
            throw std::bad_alloc();
        }
        free_blocks.reset(new free_blocks_by_size(page_size));
        // pointer to currently non-used memory
        uint8 * available = reinterpret_cast<uint8 *>(arena.get());
        // End-Of-Memory marker
        uint8 * EOM = available + arena_size;

        // split all arena on blocks and store them in free blocks table

        // first dummy border blocks
        // left technical border block to prevent block merge underflow
        block * left_border = new (available) block();
        available += left_border->size_with_header();
        // leave space to 'right border' that will prevent block merge overflow
        EOM -= block::header_size;

        // allocate blocks of maximal size first further they could be split
        block * prev_allocated_block = left_border;
        while (static_cast<size_t>(EOM - available) >= page_size) {
            block * huge_block = new (available) block(page_size - block::header_size, prev_allocated_block);
            // store block at max pos of table
            free_blocks->put_block(huge_block);
            available += huge_block->size_with_header();
            debug_assert(available < EOM);
            prev_allocated_block = huge_block;
        }
        // allocate space what's left
        uint32 leftover_size = EOM - available - block::header_size;
        if (leftover_size >= block::split_threshold) {
            block * leftover_block = new (available) block(leftover_size, prev_allocated_block);
            free_blocks->put_block(leftover_block);
            prev_allocated_block = leftover_block;
            EOM = available + leftover_block->size_with_header();
        } else {
            EOM = available;
        }
        // right technical border block to prevent block merge overflow
        block * right_border = new (EOM) block(0, prev_allocated_block);

        // mark borders as used so they will not be merged with others
        block::checkout(left_border);
        block::checkout(right_border);
        debug_only(free_blocks->test_bit_index_integrity_check());
    }


    inline memalloc::block * memalloc::merge_unused(memalloc::block * blk) noexcept {
        return blk;
    }


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
        return true;
    }


    inline size_t memalloc::reveal_actual_size(void * ptr) const noexcept {
        #if defined(ADDRESS_SANITIZER)
        return 0;
        #endif
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        debug_assert(blk->is_used());
        return blk->size();
    }


    inline void memalloc::touch(void * ptr) noexcept {
        #if defined(ADDRESS_SANITIZER)
        return;
        #endif
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        // ensure we're touching the used block
        debug_assert(blk->is_used());
        // re-link to the head to increase chances to survive
        //TODO: Implementation
    }


    template <typename ForeachFreed>
    inline void * memalloc::alloc_or_evict(size_t size, bool evict_if_necessary, ForeachFreed on_free_block) {
        #if defined(ADDRESS_SANITIZER)
        return std::malloc(size);
        #endif
        STAT_INCR(mem.num_malloc, 1);
        STAT_INCR(mem.total_requested, size);
        auto free_block_incr_evictions = [=](void * ptr) noexcept {
            STAT_INCR(mem.evictions, 1);
            on_free_block(ptr);
        };
        auto memory = alloc_or_evict_impl<decltype(free_block_incr_evictions)>(size, evict_if_necessary, free_block_incr_evictions);
        if (memory != nullptr) {
            STAT_INCR(mem.total_served, reveal_actual_size(memory));
        } else {
            STAT_INCR(mem.num_alloc_errors, 1);
            STAT_INCR(mem.total_unserved, size);
        }
        return memory;
    }


    template <typename ForeachFreed>
    inline void * memalloc::alloc_or_evict_impl(const size_t requested_size, bool evict_if_necessary, ForeachFreed on_free_block) {
        const uint32 actual_size = requested_size > block::min_memory ? requested_size + unaligned_bytes(requested_size, block::alignment) : block::min_memory;
        debug_only(free_blocks->test_bit_index_integrity_check());
        // 1. Search among the free blocks
        {
            block * found_blk = free_blocks->try_get_block(actual_size);
            if (found_blk != nullptr) {
                return block::checkout(found_blk);
            }
        }
        // 2. Try to evict existing block to free some space
        if (evict_if_necessary) {
        }
        return nullptr;
    }


    inline void * memalloc::realloc_inplace(void * ptr, const size_t new_size) noexcept {
//        #if defined(ADDRESS_SANITIZER)
//        return nullptr;
//        #endif
//        debug_assert(valid_addr(ptr));
//        debug_only(free_blocks->test_bit_index_integrity_check());
//        debug_only(used_blocks->test_bit_index_integrity_check());
//        debug_assert(valid_addr(ptr));
//        debug_assert(new_size <= max_allocation);
//        debug_assert(new_size > 0);
//
//        STAT_INCR(mem.num_realloc, 1);
//        block * blk = block::from_user_ptr(ptr);
//
//        // shrink the block
//        if (new_size <= blk->size()) {
//            block_list::unlink(blk);
//            block::unuse(blk);
//            return checkout(blk, new_size);
//        }
//
//        // extend the block on the right side
//        uint32 available = blk->size();
//        auto right = blk->right_adjacent();
//        // check if we have enough space first
//        while (available < new_size && right->is_free()) {
//            available += right->size_with_header();
//            right = right->right_adjacent();
//        }
//        if (available >= new_size) {
//            block_list::unlink(blk);
//            block::unuse(blk);
//            while (blk->size() < new_size) {
//                debug_assert(blk->right_adjacent()->is_free());
//                block_list::unlink(blk->right_adjacent());
//                blk = block::merge(blk, blk->right_adjacent());
//            }
//            debug_assert(blk->size() >= new_size);
//            return checkout(blk, new_size);
//        }
//
//        // give up
//        STAT_INCR(mem.num_realloc_errors, 1);
        return nullptr;
    }


    inline void memalloc::free(void * ptr) noexcept {
        #if defined(ADDRESS_SANITIZER)
        std::free(ptr); return;
        #endif
        debug_assert(valid_addr(ptr));
        debug_only(free_blocks->test_bit_index_integrity_check());
        STAT_INCR(mem.num_free, 1);
        block * blk = block::from_user_ptr(ptr);
//        // remove from the free_blocks_by_size table
//        block_list::unlink(blk);
//        // merge and put into free list
//        unuse(blk);
    }

} // namespace cachelot


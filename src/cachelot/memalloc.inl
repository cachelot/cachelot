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
 * There is a table grouping blocks by their size (@ref group_by_size), to serve allocations. Each cell of this table is doubly-linked list of blocks of near size.<br/>
 * The easiest way to think about grouping near size blocks  as of the sizes are powers of 2, as follows:<br/>
 *  `[256, 512, 1024, 2048 ... 2^last_power_of_2]`<br/>
 * However, additionally each range [ 2^power .. 2^(power+1) ) split by `num_sub_cells_per_power`, as follows `(num_sub_cells_per_power = 32)`:<br/>
 *  `[256, 264, 272, 280, 288, 296 ... 512, 528, 544, ... allocation_limit]`<br/>
 * The first power of 2 and num_sub_cells_per_power defines alignment of each allocation:<br/>
 *  `alignment = (2^(first_power_of_2_offset+1) - 2^(first_power_of_2_offset)) / num_sub_cells_per_power`<br/>
 * Moreover, blocks of size below 2^<sup>(first_power_of_2_offset+1)</sup> are considered as a small. The range `[0..small_block_boundary)` also split on `num_sub_cells_per_power` cells and stored in the `table[0]`.<br/>
 * Finally, table looks as follows:<br/>
 *  `[0, 8, 16, 24 ... 248, 256, 264, ... 2^last_power_of_2, 2^last_power_of_2 + cell_difference ...]`<br/>
 *
 *  ### Block split / merge and border blocks
 *
 *<br/>
 */

#ifndef CACHELOT_BITS_H_INCLUDED
#  include <cachelot/bits.h> // pow2 utils
#endif
#ifndef CACHELOT_STATS_H_INCLUDED
#  include <cachelot/stats.h>
#endif
#include <cstddef> // offsetof

namespace cachelot {

// TODO: Use bit in left_adjacent_offset as "is left block free" marker
// This will speedup many lookups


//////// constants //////////////////////////////////


    namespace const_ {
        // Important: following constants affect memalloc allocations alignment,
        // min / max allocation size and more.
        // Handle with care

        // number of second level cells per first level cell (first level cells are powers of 2 in range `[first_power_of_2_offset..last_power_of_2]`)
        static constexpr uint32 num_sub_cells_per_power = 32; // bigger value means less memory overhead but increases table size

        // First (index 1) power of 2 in block table
#if (CACHELOT_PLATFORM_BITS == 32)
        static constexpr uint32 first_power_of_2_offset = 6;  // small block is 128 bytes, aligmnet = 4
#elif (CACHELOT_PLATFORM_BITS == 64)
        static constexpr uint32 first_power_of_2_offset = 7;  // small block is 256 bytes, aligmnet = 8
#endif
        // memalloc maintains blocks of size below this boundary little bit differently
        static constexpr uint32 small_block_boundary = pow2(first_power_of_2_offset + 1);
        // maximal block size is `2^last_power_of_2`
        static constexpr uint32 last_power_of_2 = 25; // ! can not be greater than 30 (memblock::size is 31 bit)
        static constexpr uint32 num_powers_of_2_plus_zero_cell = last_power_of_2 - first_power_of_2_offset + 1;

        constexpr uint32 sub_block_diff(uint32 power) {
            // TODO: check if compiler does this optimization by itself
            // Formula: (2^(power + 1) - 2^power) / num_sub_cells_per_power)
            // bit magic: x/(2^n) <=> x >> n
            return power > 0 ? (pow2(power + 1) - pow2(power)) >> log2u(num_sub_cells_per_power) : small_block_boundary  >> log2u(num_sub_cells_per_power);
        }
    }


//////// block //////////////////////////////////////


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

        struct link_type {
            link_type * prev, * next;
        } link;             /// link for circular list of blocks in group_by_size table

        uint8 memory_[1];   /// pointer to actual memory given to user

    public:
        /// minimal allowed block size
        constexpr static const uint32 min_size = const_::small_block_boundary / const_::num_sub_cells_per_power;
        /// maximal allowed block size
        constexpr static const uint32 max_size = pow2(const_::last_power_of_2 + 1) - const_::sub_block_diff(const_::last_power_of_2);
        /// value of block alignment
        constexpr static const uint32 alignment = alignof(decltype(block::meta));
        /// size of block metadata
        constexpr static const uint32 meta_size = sizeof(meta) + sizeof(link_type);
        /// minimal block size left after split (this prevents from creating too small blocks to decrease fragmentation)
        constexpr static const uint32 split_threshold = meta_size + 64;

        /// @name constructors
        ///@{

        /// constructor (used by technical 'border' blocks)
        block() noexcept {
            meta.used = false;
            meta.size = 0;
            meta.left_adjacent_offset = 0;
            debug_only(meta.dbg_marker = DBG_MARKER);
            debug_only(link.prev = &link);
            debug_only(link.next = &link);
        }

        /// constructor used for normal blocks
        explicit block(const uint32 the_size, const block * left_adjacent_block) noexcept {
            static_assert(min_size >= alignment, "Minimal block size must be greater or equal block alignment");
            // User memory must be properly aligned
            debug_assert(unaligned_bytes(memory_, alignof(void *)) == 0);
            // check size
            debug_assert((the_size >= min_size || the_size == 0) && the_size <= max_size);
            // set debug marker
            debug_only(meta.dbg_marker = DBG_MARKER);
            debug_only(link.prev = &link);
            debug_only(link.next = &link);
            // Fill-in memory with debug pattern
            debug_only(std::memset(memory_, DBG_FILLER, the_size));
            meta.size = the_size;
            meta.used = false;
            meta.left_adjacent_offset = left_adjacent_block->size_with_meta();
            // check previous block for consistency
            debug_only(left_adjacent_block->test_check());
        }
        ///@}

        ~block(); // Must never be called

        /// amount of memory available to user
        uint32 size() const noexcept { return meta.size; }

        /// amount of memory including metadata
        uint32 size_with_meta() const noexcept { return size() + meta_size; }

        /// modify size stored in block metadata
        void set_size(const uint32 new_size) noexcept {
            debug_assert(new_size <= max_size);
            debug_assert(new_size >= min_size);
            meta.size = new_size;
        }

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
            debug_only(left->test_check());
            return const_cast<block *>(left);
        }

        /// block adjacent to the right in arena
        block * right_adjacent() const noexcept {
            // this must be either non-technical or left border block
            debug_assert(not is_right_border());
            auto this_ = reinterpret_cast<const uint8 *>(this);
            const block * right = reinterpret_cast<const block *>(this_ + size_with_meta());
            debug_only(right->test_check());
            return const_cast<block *>(right);
        }

        /// return pointer to memory available to user
        void * memory() noexcept {
            debug_assert(not is_border());
            return memory_;
        }

        /// debug only block health check (against memory corruptions)
        void test_check() const noexcept {
                            // check self
            debug_assert(   this != nullptr                                                                         ); // Yep! It is
            debug_assert(   meta.dbg_marker == DBG_MARKER                                                           );
            debug_assert(   meta.size >= min_size || is_border()                                                    );
            debug_assert(   meta.left_adjacent_offset > 0 || is_left_border()                                       );
            debug_assert(   meta.size <= max_size                                                                   );
            debug_only(     const auto this_ = reinterpret_cast<const uint8 *>(this)                                );
                            // check left
            debug_only(     if (not is_left_border()) {                                                             );
            debug_only(         auto left = reinterpret_cast<const block *>(this_ - meta.left_adjacent_offset)      );
            debug_assert(       left->meta.dbg_marker == DBG_MARKER                                                 );
            debug_assert(       reinterpret_cast<const uint8 *>(left) + left->size_with_meta() == this_             );
            debug_only(     }                                                                                       );
                            // check right
            debug_only(     if (not is_right_border()) {                                                            );
            debug_only(         auto right = reinterpret_cast<const block *>(this_ + size_with_meta())              );
            debug_assert(       right->meta.dbg_marker == DBG_MARKER                                                );
            debug_assert(       reinterpret_cast<const uint8 *>(right) - right->meta.left_adjacent_offset == this_  );
            debug_only(     }                                                                                       );
        }

        /// get block structure back from pointer given to user
        static block * from_user_ptr(void * ptr) noexcept {
            uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
            memalloc::block * result = reinterpret_cast<memalloc::block *>(u8_ptr - offsetof(memalloc::block, memory_));
            debug_assert(result->meta.dbg_marker == DBG_MARKER);
            return result;
        }

        /// mark block as used and return pointer to available memory to user
        static void * checkout(block * blk) noexcept {
            debug_only(blk->test_check());
            debug_assert(not blk->meta.used);
            blk->meta.used = true;
            // ensure that user memory is still filled with debug pattern
            return blk->memory_;
        }

        /// mark block as free
        static void unuse(block * blk) noexcept {
            debug_only(blk->test_check());
            // block borders must stay used all the time
            debug_assert(not blk->is_border());
            debug_assert(blk->is_used());
            blk->meta.used = false;
            debug_only(std::memset(blk->memory_, DBG_FILLER, blk->size()));
        }

        /// split given block `blk` to the smaller block of `new_size`, returns splited block and leftover space as a block
        static tuple<block *, block *> split(block * blk, uint32 new_size) noexcept {
            debug_only(blk->test_check());
            debug_assert(blk->is_free());
            debug_assert(not blk->is_border());
            const uint32 new_round_size = new_size > min_size ? new_size + unaligned_bytes(blk->memory_ + new_size, alignment) : min_size;
            memalloc::block * leftover = nullptr;
            if (new_round_size < blk->size() && blk->size() - new_round_size > split_threshold) {
                const uint32 old_size = blk->size();
                memalloc::block * block_after_next = blk->right_adjacent();
                blk->set_size(new_round_size);
                debug_assert(old_size - new_round_size > block::meta_size + block::alignment);
                // temporary block to pass integrity check
                debug_only(new (reinterpret_cast<uint8 *>(blk) + blk->size_with_meta()) block(0, blk));
                leftover = new (blk->right_adjacent()) memalloc::block(old_size - new_round_size - memalloc::block::meta_size, blk);
                block_after_next->meta.left_adjacent_offset = leftover->size_with_meta();
                debug_only(leftover->test_check());
            }
            debug_only(blk->test_check());
            return make_tuple(blk, leftover);
        }

        /// merge two blocks up to `max_block_size`
        static block * merge(block * left_block, block * right_block) noexcept {
            debug_only(left_block->test_check());
            debug_only(right_block->test_check());
            debug_assert(not left_block->is_border());
            debug_assert(not right_block->is_border());
            debug_assert(right_block->left_adjacent() == left_block);
            debug_assert(left_block->right_adjacent() == right_block);
            debug_assert(left_block->is_free()); debug_assert(right_block->is_free());
            debug_assert(left_block->size() + right_block->size_with_meta() <= block::max_size);
            memalloc::block * block_after_right = right_block->right_adjacent();
            left_block->set_size(left_block->size() + right_block->size_with_meta());
            //debug_only(std::memset(right_block, DBG_FILLER, sizeof(block)));
            block_after_right->meta.left_adjacent_offset = left_block->size_with_meta();
            return left_block;
        }

    private:
        friend class memalloc::block_list;
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

        /// add block `blk` to the list
        void push_front(block * blk) noexcept {
            debug_assert(not islinked(blk));
            debug_assert(not has(blk));
            debug_only(blk->test_check());
            block::link_type *  link = &blk->link;
            debug_assert(dummy_link.next->prev == &dummy_link);
            link->next = dummy_link.next;
            link->next->prev = link;
            link->prev = &dummy_link;
            dummy_link.next = link;
            debug_assert(dummy_link.prev->next == &dummy_link);
        }

        /// add block `blk` to the back of the list
        void push_back(block * blk) noexcept {
            debug_assert(not islinked(blk));
            debug_assert(not has(blk));
            debug_only(blk->test_check());
            block::link_type * link = &blk->link;
            link->prev = dummy_link.prev;
            debug_assert(link->prev->next == &dummy_link);
            link->prev->next = link;
            link->next = &dummy_link;
            dummy_link.prev = link;
            debug_assert(dummy_link.next->prev == &dummy_link);
        }

        /// remove head of the list
        block * pop_front() noexcept {
            debug_assert(not empty());
            block::link_type * link = dummy_link.next;
            debug_assert(link->prev == &dummy_link);
            unlink(link);
            return block_from_link(link);
        }

        /// remove tail of the list
        block * pop_back() noexcept {
            debug_assert(not empty());
            block::link_type * link = dummy_link.prev;
            debug_assert(link->next == &dummy_link);
            unlink(link);
            return block_from_link(link);
        }

        /// return list head of the list
        block * front() noexcept {
            debug_assert(not empty());
            return block_from_link(dummy_link.next);
        }

        /// return tail of the list
        block * back() noexcept {
            debug_assert(not empty());
            return block_from_link(dummy_link.prev);
        }

        /// find first block that satisfies requested size
        block * find_first_of(uint32 size) noexcept {
            block::link_type * node = dummy_link.next;
            while (node != &dummy_link) {
                block * blk = block_from_link(node);
                if (blk->size() >= size) {
                    unlink(blk);
                    return blk;
                } else {
                    auto left = blk->left_adjacent();
                    if (left->is_free() && left->size() + blk->size() >= size) {
                        unlink(blk);
                        unlink(left);
                        return block::merge(left, blk);
                    }
                    auto right = blk->right_adjacent();
                    if (right->is_free() && blk->size() + right->size() >= size) {
                        unlink(blk);
                        unlink(right);
                        return block::merge(blk, right);
                    }
                }
                node = node->next;
            }
            return nullptr;
        }

        /// check if block `blk` is list head
        bool is_head(block * blk) const noexcept {
            debug_assert(islinked(blk));
            debug_assert(has(blk));
            return &blk->link == dummy_link.next;
        }

        /// check if block `blk` is list tail
        bool is_tail(block * blk) const noexcept {
            debug_assert(islinked(blk));
            debug_assert(has(blk));
            return &blk->link == dummy_link.prev;
        }

        /// remove block `blk` from the any list in which it is linked
        static void unlink(block * blk) noexcept {
            debug_assert(islinked(blk));
            debug_only(blk->test_check());
            unlink(&blk->link);
        }

        /// check whether `blk` is linked into some list
        static bool islinked(block * blk) noexcept {
            return blk->link.next != &blk->link && blk->link.prev != &blk->link;
        }

        /// check whether list contains block `blk`
        bool has(block * blk) const noexcept {
            debug_assert(blk != nullptr);
            const block::link_type * node = &dummy_link;
            while (node->next != &dummy_link) {
                node = node->next;
                if (node == &blk->link) {
                    return true;
                }
                // DBG_loopbreak(1000)
            }
            return false;
        }

    private:
        static void unlink(block::link_type * link) noexcept {
            debug_assert(link != nullptr);
            // It's not allowed to mess with border blocks
            debug_assert(not block_from_link(link)->is_border());
            // Integrity check, unlink
            debug_assert(link->prev->next == link);
            link->prev->next = link->next;
            debug_assert(link->next->prev == link);
            link->next->prev = link->prev;
            debug_only(link->next = link->prev = link);
        }

        static block * block_from_link(block::link_type * link) noexcept {
            auto ui8_ptr = reinterpret_cast<uint8 *>(link);
            auto blk = reinterpret_cast<block *>(ui8_ptr - offsetof(block, link));
            debug_only(blk->test_check());
            return blk;
        }

    private:
        block::link_type dummy_link;
    };


//////// group_by_size ///////////////////////////

    /**
     * @section group_by_size Segregation
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
     *  // and so on until max_block_size
     * @endcode
     */
    class memalloc::group_by_size {
    public:
        /**
         * position in table
         */
        class position {
            /// private constructor used by group_by_size
            explicit constexpr position(const uint32 power_index, const uint32 sub_block_index)
                : pow_index(power_index), sub_index(sub_block_index) {
                /* debug_only(test_check()); */
            }
        public:
            /// constructor
            position() noexcept {
                debug_only(pow_index = std::numeric_limits<uint32>::max());
                debug_only(sub_index = std::numeric_limits<uint32>::max());
            };

            constexpr position(const position &) noexcept = default;
            position & operator= (const position &) noexcept = default;

            /// calc size class of a corresponding block
            uint32 block_size() const noexcept {
                debug_only(test_check());
                if (pow_index > 0) {
                    const uint32 power_of_2 = const_::first_power_of_2_offset + pow_index;
                    uint32 size = pow2(power_of_2) + (sub_index * const_::sub_block_diff(power_of_2));
                    debug_assert(block::min_size <= size && size <= block::max_size);
                    return size;
                } else {
                    return sub_index * block::min_size;
                }
            }

            /// return maximal available position
            static constexpr position max() noexcept {
                return position(const_::last_power_of_2 - const_::first_power_of_2_offset, const_::num_sub_cells_per_power - 1);
            }

            void debug_print() {
                std::cout << "[" << pow_index << ":" << sub_index << "]";
            }

        //private:
            /// calculate index of target list in the linear table
            uint32 absolute() const noexcept {
                const uint32 abs_index = pow_index * const_::num_sub_cells_per_power + sub_index;
                debug_assert(abs_index < block_table_size);
                return abs_index;
            }

            // next position
            position next() const noexcept {
                uint32 next_pow_index, next_sub_index;
                if (sub_index < const_::num_sub_cells_per_power - 1) {
                    next_pow_index = pow_index;
                    next_sub_index = sub_index + 1;
                } else {
                    debug_assert(pow_index < const_::last_power_of_2);
                    next_pow_index = pow_index + 1;
                    next_sub_index = 0;
                }
                return position(next_pow_index, next_sub_index);
            }


            // previous position
            position prev() const noexcept {
                uint32 prev_pow_index, prev_sub_index;
                if (sub_index > 0) {
                    prev_pow_index = pow_index;
                    prev_sub_index = sub_index - 1;
                } else {
                    debug_assert(pow_index > 0);
                    prev_pow_index = pow_index - 1;
                    prev_sub_index = 0;
                }
                return position(prev_pow_index, prev_sub_index);
            }

            bool operator<(const position & other) const noexcept { return absolute() < other.absolute(); }
            bool operator<=(const position & other) const noexcept { return absolute() <= other.absolute(); }
            bool operator>(const position & other) const noexcept { return absolute() > other.absolute(); }
            bool operator>=(const position & other) const noexcept { return absolute() >= other.absolute(); }
            bool operator==(const position & other) const noexcept { return absolute() == other.absolute(); }
            bool operator!=(const position & other) const noexcept { return absolute() != other.absolute(); }

            // debug only self-check
            void test_check() const noexcept {
                debug_assert(pow_index <= const_::last_power_of_2);
                debug_assert(sub_index < const_::num_sub_cells_per_power);
                debug_assert(absolute() < block_table_size);
            }

            // debug only check if size within this position size class
            void test_size_check(const uint32 size) const noexcept {
                debug_assert(block_size() <= size);
                if (*this < position::max()) {
                    debug_assert(size < next().block_size());
                } else {
                    debug_assert(*this == position::max());
                    debug_assert(size == block::max_size);
                    debug_assert(size == block_size());
                }
                if (pow_index > 0 || sub_index > 0) {
                    debug_assert(size > prev().block_size());
                } else {
                    debug_assert(size == block::min_size);
                    debug_assert(block_size() == block::min_size);
                }
                (void) size; // warning in Release build
            }

        //private:
            /// corresponding power of 2
            uint32 pow_index;
            /// index of sub-block within current [pow_index .. next_power_of_2) range
            uint32 sub_index;

            friend class group_by_size;
        };

        /// calculate size class by `requested_size` while searching for block
        position lookup_position_from_size(const uint32 requested_size) const noexcept {
            debug_assert(block::min_size <= requested_size && requested_size <= block::max_size);
            if (requested_size >= const_::small_block_boundary) {
                const auto roundup_size = requested_size + (pow2(log2u(requested_size) - log2u(const_::num_sub_cells_per_power))) - 1;
                uint32 power2_idx = log2u(roundup_size);
                debug_assert(power2_idx > 0);
                debug_assert(power2_idx <= const_::last_power_of_2);
                uint32 sub_block_idx = (roundup_size >> (power2_idx - log2u(const_::num_sub_cells_per_power))) - const_::num_sub_cells_per_power;
                power2_idx = power2_idx - const_::first_power_of_2_offset;
                return position(power2_idx, sub_block_idx);
            } else {
                // small block
                return position(0, requested_size >> log2u(block::min_size)).next();
            }
        }

        /// calculate size class by `requested_size` while inserting block
        position insert_position_from_size(const uint32 requested_size) const noexcept {
            debug_assert(block::min_size <= requested_size && requested_size <= block::max_size);
            if (requested_size >= const_::small_block_boundary) {
                uint32 power2_idx = log2u(requested_size);
                debug_assert(power2_idx > 0);
                debug_assert(power2_idx <= const_::last_power_of_2);
                uint32 sub_block_idx = (requested_size >> (power2_idx - log2u(const_::num_sub_cells_per_power))) - const_::num_sub_cells_per_power;
                power2_idx = power2_idx - const_::first_power_of_2_offset;
                return position(power2_idx, sub_block_idx);
            } else {
                // small block
                return position(0, requested_size >> log2u(block::min_size));
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

    public:
        group_by_size() = default;

        /// choose the side of bi-directional block list
        enum BlockAge {
            OLDEST_BLOCK,
            NEWEST_BLOCK
        };

        /// try to get block of a `requested_size`
        /// @return block pointer or `nullptr` if fail
        tuple<block *, position> try_get_block(const uint32 requested_size) noexcept {
            debug_assert(requested_size <= block::max_size);
            position pos = lookup_position_from_size(requested_size);
            debug_only(pos.test_check());
            // access indexes first, unset bits clearly indicate that corresponding size_class is empty
            if (bit_index_probe(pos)) {
                block_list & size_class = table[pos.absolute()];
                block * blk = nullptr;
                if (not size_class.empty()) {
                    // TODO: What smaller block is doing here????
                    blk = size_class.pop_front();
                    debug_only( if (blk != nullptr) {               );
                    debug_only(     blk->test_check()               );
                    debug_only(     pos.test_size_check(blk->size()));
                    debug_assert(   blk->size() >= requested_size   );
                    debug_only( }                                   );
                }
                if (size_class.empty()) {
                    bit_index_mark_empty(pos);
                }
                return make_tuple(blk, pos);
            }
            debug_assert(table[pos.absolute()].empty());
            return make_tuple(nullptr, pos);
        }

        /// try to get the biggest available block
        /// @return block pointer or `nullptr` if fail
        block * get_biggest_block(BlockAge age_of_block) noexcept {
            // accessing indexes first
            while (first_level_bit_index > 0) {
                // 1. retrieve maximal non-empty size_classs group among powers of 2
                const uint32 pow_index = bit::most_significant(first_level_bit_index);
                // 2. retrieve the biggest block among group sub-blocks
                debug_assert(second_level_bit_index[pow_index] > 0);
                const uint32 sub_index = bit::most_significant(second_level_bit_index[pow_index]);
                const position biggest_block_pos = position(pow_index, sub_index);
                // get_block_at will update bit indexes if there's no block
                block * big_block = get_block_at(biggest_block_pos, age_of_block);
                if (big_block) {
                    return big_block;
                }
            }
            return nullptr;
        }

        /// try to get block after given `pos`
        /// @return block pointer or `nullptr` if fail
        tuple<block *, position> try_get_block_after(position pos, BlockAge age_of_block) noexcept {
            const auto has_non_empty_after = [this](position p) -> bool {
                return (bit::most_significant(first_level_bit_index) > p.pow_index) || (bit::most_significant(second_level_bit_index[p.pow_index]) > p.sub_index);
            };
            const auto find_next_set = [](uint32 bit_index, uint32 current) -> uint32 {
                const auto MSB = bit::most_significant(bit_index);
                debug_assert(current < 31);
                for (uint32 bitno = current + 1; bitno <= MSB; ++bitno) { // TODO: Replace with CPU instruction
                    if (bit::isset(bit_index, bitno)) {
                        return bitno;
                    }
                }
                return 0;
            };
            auto lookup_pos = pos;
            while (has_non_empty_after(lookup_pos)) {
                debug_assert(table[lookup_pos.absolute()].empty());
                if (lookup_pos.sub_index < const_::num_sub_cells_per_power - 1) {
                    uint32 next_non_empty_sub_index = find_next_set(second_level_bit_index[lookup_pos.pow_index], lookup_pos.sub_index);
                    debug_assert(next_non_empty_sub_index > lookup_pos.sub_index || next_non_empty_sub_index == 0);
                    if (next_non_empty_sub_index != 0) {
                        lookup_pos = position(lookup_pos.pow_index, next_non_empty_sub_index);
                        block * blk = get_block_at(lookup_pos, age_of_block);
                        if (blk) {
                            return tuple<block *, position>(blk, lookup_pos);
                        }
                    }
                }
                debug_assert(lookup_pos.pow_index < group_by_size::position::max().pow_index);
                uint32 next_non_empty_power_index = find_next_set(first_level_bit_index, lookup_pos.pow_index);
                debug_assert(next_non_empty_power_index > lookup_pos.pow_index || next_non_empty_power_index == 0);
                if (next_non_empty_power_index != 0) {
                    debug_assert(second_level_bit_index[next_non_empty_power_index] > 0);
                    lookup_pos = position(next_non_empty_power_index, bit::least_significant(second_level_bit_index[next_non_empty_power_index]) - 1);
                    block * blk = get_block_at(lookup_pos, age_of_block);
                    if (blk) {
                        return tuple<block *, position>(blk, lookup_pos);
                    }
                }
            }
            return tuple<block *, position>(nullptr, lookup_pos);
        }


        /// store block `blk` at position corresponding to its size
        void put_block(block * blk) {
            position pos = insert_position_from_size(blk->size());
            put_block_at(blk, pos);
        }

        /// store block `blk` of a maximal available size at the end of the table
        void put_biggest_block(block * blk) noexcept {
            debug_assert(blk->size() == block::max_size);
            debug_assert(position::max().block_size() == blk->size());
            put_block_at(blk, position::max());
        }

        /// try to retrieve block at position `pos`, return `nullptr` if there are no blocks
        block * get_block_at(const position pos, BlockAge age_of_block) noexcept {
            debug_only(pos.test_check());
            block * blk = nullptr;
            if (bit_index_probe(pos)) {
                block_list & size_class = table[pos.absolute()];
                if (not size_class.empty()) {
                    if (age_of_block == NEWEST_BLOCK) {
                        blk = size_class.pop_front();
                    } else {
                        blk = size_class.pop_back();
                    }
                    debug_assert(not blk->is_border());
                    debug_only(blk->test_check());
                    debug_only(pos.test_size_check(blk->size()));
                    debug_assert(blk->size() >= pos.block_size());
                }
                if (size_class.empty()) {
                    bit_index_mark_empty(pos);
                }
                return blk;
            } else {
                debug_assert(table[pos.absolute()].empty());
                return nullptr;
            }
        }

        /// store block `blk` at position `pos`
        void put_block_at(block * blk, const position pos) noexcept {
            debug_only(blk->test_check());
            debug_assert(not blk->is_border());
            debug_only(pos.test_size_check(blk->size()));
            debug_only(pos.test_check());
            memalloc::block_list & size_class = table[pos.absolute()];
            size_class.push_front(blk);
            // unconditionally update bit index
            bit_index_mark_non_empty(pos);
        }

        /// determine if there are blocks after given `pos` according to the bit index
        constexpr bool has_blocks_after(position pos) const noexcept {
            return (first_level_bit_index > pos.pow_index) || (second_level_bit_index[pos.pow_index] > pos.sub_index);
        }


        void test_bit_index_integrity_check() const noexcept {
            for (size_t pow = 0; pow < const_::num_powers_of_2_plus_zero_cell; ++pow) {
                if (second_level_bit_index[pow] > 0) {
                    debug_assert(bit::isset(first_level_bit_index, pow));
                } else {
                    debug_assert(bit::isunset(first_level_bit_index, pow));
                }
                for (unsigned bit_in_subidx = 0; bit_in_subidx < 8; ++bit_in_subidx) {
                    if (bit::isunset(second_level_bit_index[pow], bit_in_subidx)) {
                        debug_assert(table[position(pow, bit_in_subidx).absolute()].empty());
                    }
                }
            }
        }

    private:
        static constexpr uint32 block_table_size = const_::num_powers_of_2_plus_zero_cell * const_::num_sub_cells_per_power;

        // all memory blocks are located in table, segregated by block size
        block_list table[block_table_size];
        // bit indexes
        uint32 second_level_bit_index[const_::num_powers_of_2_plus_zero_cell] = {0};
        // Each power of 2 (with all sub-blocks) has corresponding bit in `first_level_bit_index`, so it masks `second_level_bit_index` too
        uint32 first_level_bit_index = 0;
        static_assert(sizeof(second_level_bit_index[0]) * 8 >= const_::num_sub_cells_per_power, "Not enough bits for sub-blocks");

        friend class memalloc;
    };

//////// memalloc //////////////////////////////////////

    inline memalloc::memalloc(void * arena, const size_t arena_size) noexcept {
        debug_only(const size_t min_arena_size = sizeof(group_by_size) * 2 + sizeof(block) * 2 + 4096);
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
        used_blocks = allocate_block_table();

        // split all arena on blocks and store them in free blocks table

        // first dummy border blocks
        // left technical border block to prevent block merge underflow
        available += unaligned_bytes(available, block::alignment); // alignment
        block * left_border = new (available) block();
        available += left_border->size_with_meta();
        // leave space to 'right border' that will prevent block merge overflow
        EOM -= block::meta_size;
        EOM -= unaligned_bytes(EOM, block::alignment);

        // allocate blocks of maximal size first further they could be split
        block * prev_allocated_block = left_border;
        while (static_cast<size_t>(EOM - available) >= block::max_size + block::meta_size) {
            block * huge_block = new (available) block(block::max_size, prev_allocated_block);
            // temporarily create block on the right to pass block consistency test
            debug_only(new (reinterpret_cast<uint8 *>(huge_block) + huge_block->size_with_meta()) block(0, huge_block));
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
            debug_only(new (reinterpret_cast<uint8 *>(leftover_block) + leftover_block->size_with_meta()) block(0, leftover_block));
            free_blocks->put_block(leftover_block);
            prev_allocated_block = leftover_block;
            EOM = available + leftover_block->size_with_meta();
        } else {
            EOM = available;
        }
        // right technical border block to prevent block merge overflow
        block * right_border = new (EOM) block(0, prev_allocated_block);

        // mark borders as used so they will not be merged with others
        block::checkout(left_border);
        block::checkout(right_border);
        // remember how much memory left to user
        user_available_memory_size = reinterpret_cast<char *>(right_border) - reinterpret_cast<char *>(left_border) - block::meta_size;
    }


    inline memalloc::block * memalloc::merge_unused(memalloc::block * blk) noexcept {
        // merge left until used block or max_block_size
        while ((blk->size() < block::max_size) && blk->left_adjacent()->is_free()) {
            if (blk->size() + blk->left_adjacent()->size_with_meta() <= block::max_size) {
                block_list::unlink(blk->left_adjacent());
                blk = block::merge(blk->left_adjacent(), blk);
            } else {
                break;
            }
        }
        // merge right until used block or max_block_size
        while ((blk->size() < block::max_size) && blk->right_adjacent()->is_free()) {
            if (blk->size() + blk->right_adjacent()->size_with_meta() <= block::max_size) {
                block_list::unlink(blk->right_adjacent());
                blk = block::merge(blk, blk->right_adjacent());
            } else {
                break;
            }
        }
        return blk;
    }


    inline void memalloc::unuse(memalloc::block * & blk) noexcept {
        block::unuse(blk);
        debug_assert(not block_list::islinked(blk));
        blk = merge_unused(blk);
        free_blocks->put_block(blk);
    }


    inline void * memalloc::checkout(memalloc::block * blk, const size_t requested_size) noexcept {
        debug_assert(blk->is_free());
        debug_assert(not block_list::islinked(blk));
        debug_assert(blk->size() >= requested_size);
        block * leftover;
        tie(blk, leftover) = block::split(blk, requested_size);
        if (leftover) {
            free_blocks->put_block(leftover);
        }
        used_blocks->put_block(blk);
        stats.mem.total_requested_mem += requested_size;
        stats.mem.total_served_mem += blk->size();
        return block::checkout(blk);
    }


    inline bool memalloc::valid_addr(void * ptr) const noexcept {
        uint8 * u8_ptr = reinterpret_cast<uint8 *>(ptr);
        if (arena_begin < u8_ptr && u8_ptr < arena_end) {
            debug_only(block::from_user_ptr(ptr)->test_check());
            return true;
        } else {
            debug_assert(false);
            return false;
        }
    }


    template <typename ForeachFreed>
    inline void * memalloc::alloc_or_evict(const size_t size, bool evict_if_necessary, ForeachFreed on_free_block) noexcept {
        stats.mem.num_malloc += 1;
        debug_assert(size <= block::max_size);
        debug_assert(size > 0);
        const uint32 nsize = size > block::min_size ? static_cast<uint32>(size) : block::min_size;
        debug_only(free_blocks->test_bit_index_integrity_check());
        debug_only(used_blocks->test_bit_index_integrity_check());
        // 1. Lookup for the free block
        group_by_size::position found_blk_pos;
        {
            // 1.1. Try to get block of corresponding size from table
            block * found_blk;
            tie(found_blk, found_blk_pos) = free_blocks->try_get_block(nsize);
            if (found_blk != nullptr) {
                stats.mem.num_free_table_hits += 1;
                return checkout(found_blk, size);
            }
        }
        {
            // 1.2. Try to split the biggest available block
            block * huge_block = free_blocks->get_biggest_block(group_by_size::NEWEST_BLOCK);
            if (huge_block != nullptr) {
                debug_assert(not block_list::islinked(huge_block));
                if (huge_block->size() >= nsize) {
                    stats.mem.num_free_table_weak_hits += 1;
                    return checkout(huge_block, size);
                } else {
                    huge_block = merge_unused(huge_block);
                    if (huge_block->size() >= nsize) {
                        stats.mem.num_free_table_weak_hits += 1;
                        return checkout(huge_block, size);
                    } else {
                        free_blocks->put_block(huge_block);
                    }
                }
            }
        }

        // 2. Try to evict existing block to free some space
        if (evict_if_necessary) {
        {
            // 2.1. Lookup within same class size of used blocks
            //block_list & size_class = used_blocks->size_class_at(found_blk_pos);
            //block * used_blk = size_class.back();
            block * used_blk = used_blocks->get_block_at(found_blk_pos, group_by_size::OLDEST_BLOCK);
            if (used_blk != nullptr) {
                debug_assert(used_blk->size() >= nsize);
                on_free_block(used_blk->memory());
                block::unuse(used_blk);
                stats.mem.num_used_table_hits += 1;
                return checkout(used_blk, size);
            }
        }
        {
            // 2.2. Try to evict bigger available block
            block * used_blk;
            tie(used_blk, found_blk_pos) = used_blocks->try_get_block_after(found_blk_pos, group_by_size::OLDEST_BLOCK);
            if (used_blk != nullptr) {
                debug_assert(used_blk->size() >= nsize);
                on_free_block(used_blk->memory());
                block::unuse(used_blk);
                stats.mem.num_used_table_weak_hits += 1;
                return checkout(used_blk, size);
            }
        }
        {
            // 2.3 Merge one of biggest used blocks
            block * big_used_blk;
            big_used_blk = used_blocks->get_biggest_block(group_by_size::OLDEST_BLOCK);
            if (big_used_blk != nullptr) {
                on_free_block(big_used_blk->memory());
                block::unuse(big_used_blk);
                big_used_blk = merge_unused(big_used_blk);
                // TODO: This particular merge breaks O(1) complexity
                while (big_used_blk->size() < nsize) {
                    uint32 available_left = big_used_blk->left_adjacent()->size();
                    uint32 available_right = big_used_blk->right_adjacent()->size();
                    debug_assert(available_left != 0 || available_right != 0);
                    if (available_right > available_left) {
                        block * right = big_used_blk->right_adjacent();
                        if (right->is_used()) {
                            on_free_block(right->memory());
                            block::unuse(right);
                        }
                        block_list::unlink(right);
                        big_used_blk = block::merge(big_used_blk, right);
                        stats.mem.num_used_table_merges += 1;
                    } else {
                        block * left = big_used_blk->left_adjacent();
                        if (left->is_used()) {
                            on_free_block(left->memory());
                            block::unuse(left);
                        }
                        block_list::unlink(left);
                        big_used_blk = block::merge(left, big_used_blk);
                        stats.mem.num_used_table_merges += 1;
                    }
                }
                if (big_used_blk->size() >= nsize) {
                    return checkout(big_used_blk, size);
                } else {
                    free_blocks->put_block(big_used_blk);
                }
            }
        }
        }
        stats.mem.total_requested_mem -= size;
        stats.mem.total_unserved_mem += size;
        stats.mem.num_errors += 1;
        return nullptr;
    }


    inline void * memalloc::try_realloc_inplace(void * ptr, const size_t new_size) noexcept {
        debug_only(free_blocks->test_bit_index_integrity_check());
        debug_only(used_blocks->test_bit_index_integrity_check());
        stats.mem.num_realloc += 1;
        debug_assert(valid_addr(ptr));
        debug_assert(new_size <= block::max_size);
        debug_assert(new_size > 0);
        block * blk = block::from_user_ptr(ptr);

        if (blk->size() >= new_size) {
            return ptr;
        }

        // check if we have free space on the right
        uint32 available_on_the_right = 0;
        auto right = blk->right_adjacent();
        while (right->is_free() && available_on_the_right + blk->size() < new_size) {
            available_on_the_right += right->size_with_meta();
            right = right->right_adjacent();
        }
        if (available_on_the_right + blk->size() >= new_size) {
            block::unuse(blk);
            block_list::unlink(blk);
            while (blk->size() < new_size) {
                debug_assert(blk->right_adjacent()->is_free());
                block_list::unlink(blk->right_adjacent());
                blk = block::merge(blk, blk->right_adjacent());
            }
            debug_assert(blk->size() >= new_size);
            return checkout(blk, new_size);
        }
        stats.mem.total_unserved_mem += new_size;
        return nullptr;
    }


    inline void memalloc::free(void * ptr) noexcept {
        debug_only(free_blocks->test_bit_index_integrity_check());
        debug_only(used_blocks->test_bit_index_integrity_check());
        stats.mem.num_free += 1;
        debug_assert(valid_addr(ptr));
        block * blk = block::from_user_ptr(ptr);
        debug_assert(block_list::islinked(blk));
        // remove from the used_block group_by_size table
        block_list::unlink(blk);
        debug_assert(not block_list::islinked(blk));
        // merge and put into free list
        unuse(blk);
    }


} // namespace cachelot


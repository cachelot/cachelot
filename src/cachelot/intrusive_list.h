#ifndef CACHELOT_INTRUSIVE_LIST_H_INCLUDED
#define CACHELOT_INTRUSIVE_LIST_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

/// @ingroup common
/// @{

namespace cachelot {

    class intrusive_list_link {
    protected:
        intrusive_list_link * prev, * next;
    };

    /**
     * Simple doubly linked circular intrusive list
     * It allows to remove items outside of list (@see intrusive_list::unlink())
     *
     * @tparam T - stored item type
     */
    template <class T, intrusive_list_link T::*LinkOffset>
    class intrusive_list {
        /// Link interface
        struct link_type : public intrusive_list_link {
            typedef T * pointer;
            link_type() noexcept {}
            explicit link_type(pointer fromItem);
            pointer get_item() noexcept;
            bool is_linked() const noexcept;
        };

    public:
        typedef T value_type;
        typedef T * pointer;
        /// constructor
        constexpr intrusive_list() noexcept : dummy_link({&dummy_link, &dummy_link}) {}

        /// add block `blk` to the list
        void push_front(pointer blk) noexcept {
            debug_assert(not islinked(blk));
            debug_assert(not has(blk));
            link_type *  link = &blk->link;
            debug_assert(dummy_link.next->prev == &dummy_link);
            link->next = dummy_link.next;
            link->next->prev = link;
            link->prev = &dummy_link;
            dummy_link.next = link;
            debug_assert(dummy_link.prev->next == &dummy_link);
        }

        /// add block `blk` to the back of the list
        void push_back(pointer blk) noexcept {
            debug_assert(not islinked(blk));
            debug_assert(not has(blk));
            link_type * link = &blk->link;
            link->prev = dummy_link.prev;
            debug_assert(link->prev->next == &dummy_link);
            link->prev->next = link;
            link->next = &dummy_link;
            dummy_link.prev = link;
            debug_assert(dummy_link.next->prev == &dummy_link);
        }

        /// remove head of the list
        pointer pop_front() noexcept {
            debug_assert(not empty());
            link_type * link = dummy_link.next;
            debug_assert(link->prev == &dummy_link);
            unlink(link);
            return block_from_link(link);
        }

        /// remove tail of the list
        pointer pop_back() noexcept {
            debug_assert(not empty());
            link_type * link = dummy_link.prev;
            debug_assert(link->next == &dummy_link);
            unlink(link);
            return block_from_link(link);
        }

        /// return list head of the list
        pointer front() noexcept {
            debug_assert(not empty());
            return block_from_link(dummy_link.next);
        }

        /// return tail of the list
        pointer back() noexcept {
            debug_assert(not empty());
            return block_from_link(dummy_link.prev);
        }

        /// check if block `blk` is list head
        bool is_head(pointer blk) const noexcept {
            debug_assert(islinked(blk));
            debug_assert(has(blk));
            return &blk->link == dummy_link.next;
        }

        /// check if block `blk` is list tail
        bool is_tail(pointer blk) const noexcept {
            debug_assert(islinked(blk));
            debug_assert(has(blk));
            return &blk->link == dummy_link.prev;
        }

        /// remove block `blk` from the any list in which it is linked
        static void unlink(pointer blk) noexcept {
            debug_assert(islinked(blk));
            unlink(&blk->link);
            debug_assert(not islinked(blk));
        }

        /// check whether `blk` is linked into some list
        static bool islinked(pointer blk) noexcept {
            return blk->link.next != &blk->link && blk->link.prev != &blk->link;
        }

        /// check whether list contains block `blk`
        bool has(pointer blk) const noexcept {
            debug_assert(blk != nullptr);
            const link_type * node = &dummy_link;
            while (node->next != &dummy_link) {
                node = node->next;
                if (node == &blk->link) {
                    return true;
                }
                // DBG_loopbreak(1000)
            }
            return false;
        }

        /// check if list is empty
        bool empty() const noexcept {
            return dummy_link.next == &dummy_link;
        }

    private:
        static void unlink(link_type * link) noexcept {
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

        static pointer block_from_link(link_type * link) noexcept {
            auto ui8_ptr = reinterpret_cast<uint8 *>(link);
            auto blk = reinterpret_cast<pointer>(ui8_ptr - offsetof(value_type, link_type));
            return blk;
        }

    private:
        link_type dummy_link;
    };

} // namespace cachelot

/// @}

#endif // CACHELOT_INTRUSIVE_LIST_H_INCLUDED

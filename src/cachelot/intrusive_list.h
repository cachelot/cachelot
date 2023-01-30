#ifndef CACHELOT_INTRUSIVE_LIST_H_INCLUDED
#define CACHELOT_INTRUSIVE_LIST_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#ifndef CACHELOT_COMMON_H_INCLUDED
#  include <cachelot/common.h>
#endif
#ifndef BOOST_INTRUSIVE_DETAIL_PARENT_FROM_MEMBER_HPP_INCLUDED
#  include <boost/intrusive/detail/parent_from_member.hpp>
#  define BOOST_INTRUSIVE_DETAIL_PARENT_FROM_MEMBER_HPP_INCLUDED
#endif


namespace cachelot {

    struct intrusive_list_node {
        intrusive_list_node * prev, * next;
    };

    /**
     * Simple doubly linked circular intrusive list. Not STL compliant.
     *
     * Intrusive means stored items already contain list nodes and
     * intrusive_list does not holds items itself, only pointers to them.
     * User is responsible for providing pointers to the non-temporary items.
     *
     * @tparam T - stored item type
     * @tparam LinkPonter - pointer to the *intrusive_list_node* member
     * @ingroup common
     */
    template <class T, intrusive_list_node T::*LinkPonter>
    class intrusive_list {
        // Note: we don't use boost::intrusive_list
        // Boost intrusive_list hooks must be properly initilized all the time. It's not our case (memory manager internal nodes).
        typedef intrusive_list_node node_type;
    public:
        typedef T value_type;
        typedef T * pointer;
        /// constructor
        constexpr intrusive_list() noexcept : dummy_link({&dummy_link, &dummy_link}) {}

        /// return head of the list
        pointer front() noexcept {
            debug_assert(! empty());
            return pointer_from_link(dummy_link.next);
        }

        /// return tail of the list
        pointer back() noexcept {
            debug_assert(! empty());
            return pointer_from_link(dummy_link.prev);
        }

        /// add block `item` to the list
        void push_front(pointer item) noexcept {
            auto link = &(item->*LinkPonter);
            link->next = dummy_link.next;
            link->next->prev = link;
            link->prev = &dummy_link;
            dummy_link.next = link;
        }

        /// add block `item` to the back of the list
        void push_back(pointer item) noexcept {
            node_type * link = &(item->*LinkPonter);
            link->prev = dummy_link.prev;
            link->prev->next = link;
            link->next = &dummy_link;
            dummy_link.prev = link;
        }

        /// remove head of the list
        pointer pop_front() noexcept {
            debug_assert(! empty());
            node_type * link = dummy_link.next;
            unlink(link);
            return pointer_from_link(link);
        }

        /// remove tail of the list
        pointer pop_back() noexcept {
            debug_assert(! empty());
            node_type * link = dummy_link.prev;
            debug_assert(link->next == &dummy_link);
            unlink(link);
            return pointer_from_link(link);
        }

        /// remove `item` from the list
        void remove(pointer item) noexcept {
            debug_assert(has(item));
            unlink(item);
        }

        /// check if block `item` is list head
        bool is_head(pointer item) const noexcept {
            debug_assert(is_linked(item));
            debug_assert(has(item));
            return &(item->*LinkPonter) == dummy_link.next;
        }

        /// check if block `item` is list tail
        bool is_tail(pointer item) const noexcept {
            debug_assert(is_linked(item));
            debug_assert(has(item));
            return &item->link == dummy_link.prev;
        }

        /// move `item` to front
        void move_front(pointer item) noexcept {
            debug_assert(has(item));
            if (is_head(item)) {
                return;
            }
            node_type * link = &(item->*LinkPonter);
            auto prev = link->prev;
            // relink this with prev->prev
            prev->prev->next = link;
            link->prev = prev->prev;
            auto next = link->next;
            // relink prev with next
            next->prev = prev;
            prev->next = next;
            // relink this with prev
            prev->prev = link;
            link->next = prev;
        }

        /// remove block `item` from the any list in which it is linked
        static void unlink(pointer item) noexcept {
            debug_assert(is_linked(item));
            unlink(&(item->*LinkPonter));
        }

        /// check whether `item` is linked into some list
        static bool is_linked(pointer item) noexcept {
            auto link = &(item->*LinkPonter);
            return link->next != link && link->prev != link;
        }

        /// check whether list contains block `item`
        bool has(pointer item) const noexcept {
            debug_assert(item != nullptr);
            if (! is_linked(item)) {
                return false;
            }
            const node_type * node = &dummy_link;
            while (node->next != &dummy_link) {
                node = node->next;
                if (node == &(item->*LinkPonter)) {
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
        static void unlink(node_type * link) noexcept {
            debug_assert(link != nullptr);
            // Integrity check, unlink
            debug_assert(link->prev->next == link);
            link->prev->next = link->next;
            debug_assert(link->next->prev == link);
            link->next->prev = link->prev;
            debug_only(link->next = link->prev = link);
        }

        static pointer pointer_from_link(node_type * link) noexcept {
            // we don't want to mess with ABI by ourselves
            return boost::intrusive::detail::parent_from_member<T, intrusive_list_node>(link, LinkPonter);
        }

    private:
        node_type dummy_link;
    };

} // namespace cachelot

#endif // CACHELOT_INTRUSIVE_LIST_H_INCLUDED

#ifndef CACHELOT_MPSC_QUEUE_H_INCLUDED
#define CACHELOT_MPSC_QUEUE_H_INCLUDED

#include <atomic>

/**
 * @ingroup common
 * @{
 */

namespace cachelot {

    /**
     * Multiple producer single consumer concurrent intrusive queue
     * Based on [Dmitry Vyukov's algorithm](http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue)
     * mpsc_queue expects that contained item class `T` is derived from mpsc_queue::node
     */
    template <class T>
    class mpsc_queue {
        struct queue_node {
            std::atomic<queue_node *> next;
            queue_node() {} // doesn't do anything
        };
    public:
        typedef typename std::decay<T>::type * pointer;
        using node = queue_node;
        // ctor
        mpsc_queue() noexcept
            : m_head(&stub)
            , m_tail(&stub) {
            stub.next = nullptr;
            static_assert(std::is_base_of<mpsc_queue<T>::node, T>::value, "Stored items class must be derived of mpsc_queue::node");
        }

        // disallow copying
        template <class Y> mpsc_queue(const mpsc_queue<Y> &) = delete;
        template <class Y> mpsc_queue<Y> & operator= (const mpsc_queue<Y> &) = delete;

        /**
         * @brief add new item to the queue
         *
         * @note: as mpsc_queue is intrusive `new_item` should not be stored in other queue at the moment enqueue() is called
         * enqueue() is thread safe
         */
        void enqueue(pointer new_item) noexcept {
            enqueue_node(reinterpret_cast<queue_node *>(new_item));
        }

        /**
         * @brief try to get item from the queue, returns `nullptr` if queue is empty
         *
         * @note function can be called from only one thread,
         * that is dequeue() can be concurrently called with enqueue(), but not with itself
         */
        pointer dequeue() noexcept {
            queue_node * tail = m_tail;
            queue_node * next = tail->next;
            if (tail == &stub) {
                // if there is only one stub node
                if (next == nullptr) { return nullptr; }
                // else advance tail beyond stub node
                m_tail = next;
                tail = next;
                next = next->next;
            }
            if (next != nullptr) {
                m_tail = next;
                return reinterpret_cast<pointer>(tail);
            }
            if (tail == m_head.load(std::memory_order_relaxed)) {
                enqueue_node(&stub);
                next = tail->next;
                if (next != nullptr) {
                    m_tail = next;
                    return reinterpret_cast<pointer>(tail);
                }
            }
            return nullptr;
        }

    private:
        void enqueue_node(queue_node * node) {
            node->next = nullptr;
            queue_node * prev = m_head.exchange(node);
            debug_assert(prev->next == nullptr);
            prev->next = node;
        }

    private:
        std::atomic<queue_node *> m_head;
        queue_node * m_tail;
        queue_node stub;
    };

} // namespace cachelot

/// @}

#endif // CACHELOT_MPSC_QUEUE_H_INCLUDED

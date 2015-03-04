#ifndef CACHELOT_POOL_ALLOCATOR_H_INCLUDED
#define CACHELOT_POOL_ALLOCATOR_H_INCLUDED

#include <cachelot/bits.h>  // is_pow2
#include <atomic>

/**
 * @ingroup common
 * @{
 */

namespace cachelot {

    /**
     * pool_allocator is thead safe pool based class allocator
     * it's synchronised with atomic CAS instructions rather then with OS mutex
     *
     * @tparam T - Item type
     * @tparam Limit - Maximum number of items kept in the pool
     *
     * @note pool_allocator is not STL compliant allocator
     *
     * Implementation is based on [Dmitry Vyukov's MPMC bounded queue](http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)
     */
    template <class T, size_t Limit>
    class pool_allocator {
        static_assert(Limit > 1 && ispow2(Limit), "Limit must be the power of 2");
        // disallow copying
        pool_allocator(const pool_allocator & ) = delete;
        pool_allocator & operator=(const pool_allocator & ) = delete;

    public:
        typedef typename std::remove_reference<T>::type * pointer;

    public:
        /// constructor
        pool_allocator()
            : free_items(new free_node[Limit]) {
            static_assert(std::is_trivially_destructible<T>::value, "Item must be trivially destructible");
            for (size_t i = 0; i < Limit; i += 1) {
                free_items[i].sequence.store(i, std::memory_order_relaxed);
            }
            add_pos.store(0, std::memory_order_relaxed);
            remove_pos.store(0, std::memory_order_relaxed);
        }

        /// destructor
        ~pool_allocator() {
            pointer item = get_from_pool();
            while (item != nullptr) {
                ::operator delete(item);
                item = get_from_pool();
            }
        }

        /**
         * allocate memory from pool if possible otherwise from the system
         * and create new instance of a type `T`
         */
        template <typename ... Args>
        pointer create(Args && ... args) {
            void * memory = get_from_pool();
            if (memory == nullptr) {
                memory = ::operator new(sizeof(T));
            }
            debug_assert(unaligned_bytes(memory, alignof(T)) == 0);
            return new (memory) T(std::forward<Args>(args) ...);
        }

        /**
         * destroy the item `ptr` and put freed memory into the pool
         * if pool is full then memory will be released back to the system
         */
        void destroy(pointer ptr) {
            if (put_into_pool(ptr) == false) {
                ::operator delete(ptr);
            }
        }

    private:
        /// try to get pre-allocated item from the pool
        pointer get_from_pool() noexcept {
            free_node * node;
            size_t pos = remove_pos.load(std::memory_order_relaxed);
            for (;;) {
                node = &free_items[pos & free_items_mask];
                ptrdiff_t diff = node->sequence.load(std::memory_order_acquire) - (pos + 1);
                if (diff == 0) {
                    if (remove_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return nullptr;
                } else {
                    pos = remove_pos.load(std::memory_order_relaxed);
                }
            }
            node->sequence.store(pos + free_items_mask + 1, std::memory_order_release);
            return node->item;
        }

        /// try to put item in the pool, return `false` if pool is full
        bool put_into_pool(pointer ptr) noexcept {
            free_node * node;
            size_t pos = add_pos.load(std::memory_order_relaxed);
            for (;;) {
                node = &free_items[pos & free_items_mask];
                ptrdiff_t diff = node->sequence.load(std::memory_order_acquire) - pos;
                if (diff == 0) {
                    if (add_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false;
                } else {
                    pos = add_pos.load(std::memory_order_relaxed);
                }
            }
            node->item = ptr;
            node->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

    private:
        struct free_node {
            std::atomic<size_t> sequence;
            pointer item;
        };
        static constexpr size_t free_items_mask = Limit - 1;
        std::unique_ptr<free_node[]> free_items;
        alignas(cpu_l1d_cache_line) std::atomic<size_t> add_pos;
        alignas(cpu_l1d_cache_line) std::atomic<size_t> remove_pos;
    };

} // namespace cachelot

/// @}

#endif // CACHELOT_POOL_ALLOCATOR_H_INCLUDED

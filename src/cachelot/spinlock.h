#ifndef CACHELOT_SPINLOCK_H_INCLUDED
#define CACHELOT_SPINLOCK_H_INCLUDED

#include <atomic>

/// @ingroup common
/// @{

namespace cachelot {

    /**
     * spinlock is a synchronisation primitive based on infinite loop with atomic flag check
     *
     * spinlock uses infinite loop in @lock operation rather then native OS synchronisation.
     * It may give better performance on small locks by reducing latency, but for a long time locks 
     * (like IO waits) it is just a waste of CPU cycles.
     */
    class spinlock {
        static constexpr bool NOT_LOCKED = false;
    public:
        spinlock()
            : m_lock_flag(ATOMIC_FLAG_INIT)
        {}

        /// acquire lock, wait until succeed
        void lock() noexcept {
            bool locked = try_lock();
            while (not locked) {
                pause_cpu();
                locked = try_lock();
            }
        }

        /// try to acquire lock, immediately return result
        bool try_lock() noexcept {
            return NOT_LOCKED == m_lock_flag.test_and_set(std::memory_order_acquire);
        }
        
        /// release lock
        void unlock() noexcept {
            m_lock_flag.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_lock_flag;
    };
 

} // namespace cachelot

/// @}

#endif // CACHELOT_SPINLOCK_H_INCLUDED

#ifndef CACHELOT_SPINLOCK_H_INCLUDED
#define CACHELOT_SPINLOCK_H_INCLUDED

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
    class spinlock : protected atomic_lock {
    public:
        /// acquire lock, wait until succeed
        void lock() throw() { 
            bool acquired;
            do { 
                acquired = acquire(); 
            } while (!acquired);
        }

        /// try to acquire lock, immediate return result
        bool try_lock() throw() { return acquire(); }
        
        /// release lock
        void unlock() throw() { release(); }
    };
 

} // namespace cachelot

/// @}

#endif // CACHELOT_SPINLOCK_H_INCLUDED

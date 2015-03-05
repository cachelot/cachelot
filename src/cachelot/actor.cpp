#include <cachelot/cachelot.h>
#include <cachelot/actor.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <emmintrin.h> // _mm_pause
#include <boost/thread/tss.hpp> // Xcode clang doesn't supoort thread_local

namespace cachelot {


//////// Actor::Thread //////////////////////////////


    class Actor::Thread {
        static constexpr uint32 max_wait_spin_count = 10000;
    public:
        Thread(Actor::ActFunction act_function, Actor & self)
            : m_self(self)
            , m_function(act_function) {
        }

        ~Thread() {
            stop();
            join();
        }

        void start() noexcept {
            debug_assert(not m_thread.joinable());
            m_is_waiting.store(false, std::memory_order_relaxed);
            m_is_interrupted.store(false, std::memory_order_relaxed);
            m_thread = std::thread([this]() {
                uint32 wait_spins;
                while (not m_is_interrupted.load(std::memory_order_relaxed)) {
                    bool has_work = m_function(&m_self);
                    if (has_work) {
                        wait_spins = 0;
                        continue;
                    }
                    // wait in spinlock
                    pause();
//                    wait_spins += 1;
//                    if (wait_spins >= max_wait_spin_count) {
//                        continue;
//                    }
//                    // important check as termination could be requested from the running actors
//                    if (not m_is_interrupted.load(std::memory_order_relaxed)) {
//                        // wait in kernel lock
//                        std::unique_lock<decltype(m_wait_mutex)> lck(m_wait_mutex);
//                        debug_assert(not m_is_waiting);
//                        m_is_waiting.store(true, std::memory_order_relaxed);
//                        m_wait_condition.wait(lck);
//                    }
//                    wait_spins = 0;
//                    m_is_waiting.store(false, std::memory_order_relaxed);
                }
            });
        }

        void stop() noexcept {
            m_is_interrupted.store(true);
            notify();
        }

        void notify() noexcept {
            if (/* likely */ not m_is_waiting.load(std::memory_order_relaxed)) {
                return;
            }
            m_wait_condition.notify_one();
        }

        void join() noexcept {
            try {
                if (m_thread.joinable()) {
                    m_thread.join();
                }
            } catch (...) {
                /* swallow exception */
            }
        }

        void pause() noexcept {
            _mm_pause();
        }

        bool interrupted() noexcept {
            return m_is_interrupted.load(std::memory_order_relaxed);
        }

    private:
        Actor & m_self;
        std::thread m_thread;
        Actor::ActFunction m_function;
        std::mutex m_wait_mutex;
        std::condition_variable m_wait_condition;
        std::atomic_bool m_is_waiting;
        std::atomic_bool m_is_interrupted;
    };


//////// Actor //////////////////////////////////////

    Actor::Actor(bool (Actor::*act_function)() noexcept)
        : std::enable_shared_from_this<Actor>()
        , m_thread(new Thread(act_function, *this))
        , __me(this) {
    }

    Actor::~Actor() {
        // empty mailbox
        Message * m = m_mailbox.dequeue();
        while (m != nullptr) {
            delete m;
            m = m_mailbox.dequeue();
        }
    }

    void Actor::notify() noexcept { m_thread->notify(); }

    void Actor::start() noexcept { m_thread->start(); }

    void Actor::stop() noexcept { m_thread->stop(); }

    void Actor::join() noexcept { m_thread->join(); __me.reset(); }

    bool Actor::interrupted() noexcept { return m_thread->interrupted(); }

    void Actor::pause() noexcept { m_thread->pause(); }


} // namespace cachelot


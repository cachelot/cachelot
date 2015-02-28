#include <cachelot/cachelot.h>
#include <cachelot/actor.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <emmintrin.h> // _mm_pause

namespace cachelot {


//////// ActorThreadImpl ////////////////////////////


    class ActorThread::ActorThreadImpl {
        static constexpr uint32 max_wait_spin_count = 10000;
    public:
        ActorThreadImpl(MainFunction fun)
            : m_func(fun) {
        }

        ~ActorThreadImpl() {
            stop();
            join();
        }

        void start() noexcept {
            debug_assert(not m_thread.joinable());
            m_is_waiting = false;
            m_is_interrupted = false;
            m_thread = std::thread([this]() {
                uint32 wait_spins;
                while (not m_is_interrupted) {
                    bool has_work = false;
                    for (auto actor: m_actors) {
                        has_work |= actor->act();
                    }
                    has_work |= m_func();
                    if (has_work) {
                        wait_spins = 0;
                        continue;
                    }
                    // wait in spinlock
                    _mm_pause();
                    wait_spins += 1;
                    if (wait_spins >= max_wait_spin_count) {
                        continue;
                    }
                    // wait in kernel lock
                    std::unique_lock<decltype(m_wait_mutex)> lck;
                    m_is_waiting = true;
                    m_wait_condition.wait(lck);
                    wait_spins = 0;
                }
            });
        }

        void stop() noexcept {
            m_is_interrupted = true;
            notify();
        }

        void notify() noexcept {
            if (/* likely */ not m_is_waiting) {
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

        void attach(Actor * actor) noexcept {
            debug_assert(not m_thread.joinable() && "Can't attach actor after thread start");
            debug_only(for (auto a: m_actors) { debug_assert(a != actor); });
            m_actors.push_back(actor);
        }

        void detach(Actor * actor) {
            for (auto a = m_actors.begin(); a != m_actors.end(); ++a) {
                if (*a == actor) {
                    *a = m_actors.back();
                    m_actors.pop_back();
                }
            }
            debug_assert(false && "attempt to detach non-attached Actor");
        }

    private:
        std::vector<Actor *> m_actors;
        std::thread m_thread;
        MainFunction m_func;
        std::mutex m_wait_mutex;
        std::condition_variable m_wait_condition;
        std::atomic_bool m_is_waiting;
        std::atomic_bool m_is_interrupted;
    };


//////// ActorThread ////////////////////////////////


    ActorThread::ActorThread(MainFunction func)
        : m_impl(new ActorThreadImpl(func)) {
    }

    void ActorThread::start() noexcept { m_impl->start(); }

    /// request thread to stop execution
    void ActorThread::stop() noexcept { m_impl->stop(); }

    /// wait for thread completion
    void ActorThread::join() noexcept { m_impl->join(); }


//////// Actor //////////////////////////////////////


    /// notify owner ActorThread to wake it up
    void Actor::notify() noexcept {
        m_execution_thread.m_impl->notify();
    }

    /// attach this actor to the thread
    void Actor::attach() noexcept {
        m_execution_thread.m_impl->attach(this);
    }

    /// detach this actor from the thread
    void Actor::detach() noexcept {
        m_execution_thread.m_impl->detach(this);
    }

} // namespace cachelot


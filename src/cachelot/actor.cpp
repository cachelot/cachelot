#include <cachelot/cachelot.h>
#include <cachelot/actor.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <emmintrin.h> // _mm_pause
#include <boost/thread/tss.hpp> // Xcode clang doesn't supoort thread_local

namespace cachelot {


//////// Actor::Impl //////////////////////////////


    class Actor::Impl {
        static constexpr uint32 max_wait_spin_count = 10000;
    public:
        Impl(Actor::ActFunction act_function, Actor & self)
            : m_self(self)
            , m_function(act_function)
            , m_thread() {
        }

        ~Impl() {
            stop();
            join();
            // empty mailbox
            auto * m = m_mailbox.dequeue();
            while (m != nullptr) {
                delete m;
                m = m_mailbox.dequeue();
            }
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

        Message * receive_message() noexcept {
            return m_mailbox.dequeue();
        }

        void send_to(std::shared_ptr<Impl> to, Message * msg) {
            debug_assert(to);
            debug_assert(msg);
            to->m_mailbox.enqueue(msg);
        }

    private:
        Actor & m_self;
        Actor::ActFunction m_function;
        std::thread m_thread;
        Mailbox m_mailbox;
        std::mutex m_wait_mutex;
        std::condition_variable m_wait_condition;
        std::atomic_bool m_is_waiting;
        std::atomic_bool m_is_interrupted;
    };


//////// Actor //////////////////////////////////////

    Actor::Actor(bool (Actor::*act_function)() noexcept)
        : _impl(new Impl(act_function, *this)) {
    }

    Actor::~Actor() {}

    void Actor::notify() noexcept {
        if(_impl) _impl->notify();
    }

    void Actor::start() noexcept {
        debug_assert(_impl);
        _impl->start();
    }

    void Actor::stop() noexcept {
        if(_impl) _impl->stop();
    }

    void Actor::join() noexcept {
        if(_impl) _impl->join();
    }

    bool Actor::interrupted() noexcept {
        return _impl ? _impl->interrupted() : true;
    }

    void Actor::pause() noexcept {
        debug_assert(_impl);
        _impl->pause();
    }

    std::unique_ptr<Actor::Message> Actor::receive_message() noexcept {
        if (_impl) {
            std::unique_ptr<Actor::Message> msg(_impl->receive_message());
            return std::move(msg);
        }
        return nullptr;
    }

    void Actor::impl_send_to(std::shared_ptr<Actor::Impl> to, Actor::Message * msg) noexcept {
        _impl->send_to(to, msg);
    }



} // namespace cachelot


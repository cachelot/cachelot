#include <cachelot/common.h>
#include <cachelot/actor.h>

namespace cachelot {

    Actor::Actor() noexcept
        : on_message()
        , on_idle()
        , m_thread()
        , m_mailbox()
        , m_is_interrupted(false)
        , __time_to_sleep() {
    }

    void Actor::start() {
        static const auto min_sleep_quant = chrono::nanoseconds(500);
        static const auto max_sleep_quant = chrono::milliseconds(50);
        debug_assert(not m_thread.joinable()); // check if thread is not running
        m_is_interrupted.store(false, std::memory_order_relaxed);
        m_thread = std::thread([=](){
            __time_to_sleep = min_sleep_quant;
            do {
                // check for incoming messages first
                bool has_messages = false;
                Message * msg = m_mailbox.dequeue();
                while (msg != nullptr) {
                    has_messages = true;
                    auto reply = on_message(this, *msg);
                    if (not reply) {
                        Message::Dispose(msg);
                    }
                    msg = m_mailbox.dequeue();
                }
                // execute main function
                bool has_work = on_idle(this);
                if (has_work or has_messages) {
                    // set sleep time to minimum to have better response
                    __time_to_sleep = min_sleep_quant;
                    continue;
                } else {
                    std::this_thread::sleep_for(__time_to_sleep);
                    // gradually increase sleep time if there is no work to do
                    __time_to_sleep = std::max(__time_to_sleep, __time_to_sleep * 2);
                    if (__time_to_sleep > max_sleep_quant) {
                        __time_to_sleep = max_sleep_quant;
                    }
                }
            } while (not m_is_interrupted.load());
        });

    }


    void Actor::stop() noexcept {
        m_is_interrupted.store(true, std::memory_order_relaxed);
    }


    void Actor::wait_complete() noexcept {
        try {
            if (m_thread.joinable()) {
                m_thread.join();
            }
        } catch (...) {}
    }


} // namespace cachelot


#ifndef CACHELOT_ACTOR_H_INCLUDED
#define CACHELOT_ACTOR_H_INCLUDED

#include <cachelot/atom.h>
#include <cachelot/bits.h>  // unaligned_bytes
#include <cachelot/mpsc_queue.h> // mailbox

/**
 * @defgroup actor Lightweight Erlang actor model
 * @par Example
 * @snippet src/unit_test/test_actor.cpp Simple calculator actors
 * @{
 */

namespace { namespace test_actor { struct test_message_arguments; } }

namespace cachelot {

    /**
     * Actor is lightweight process executed cooperatively in thread
     * Actors are able to communicate with each other via sending asynchronous messages
     *
     * Specific actor implementation class must be inherited from `Actor` and provide two
     * callbacks: one for periodical executing in main loop and one for incoming messages
     * @see Actor::Message
     * @see on_message and on_idle
     * @note It is possible to implement state machine by changing `on_message` callback
     */
    class Actor : public std::enable_shared_from_this<Actor> {
        class MessageBase;
        /// private constructor
        explicit Actor(bool (Actor::*act_function)() noexcept);
    public:
        class Message;

        typedef std::weak_ptr<Actor> WeakPtr;

        typedef std::unique_ptr<Message> UniqueMessagePtr;

        typedef std::function<bool (Actor *) noexcept> ActFunction;

        /**
         * constructor
         *
         * @tparam ActorType - derived class implements
         * @code
         * class MyActor : Actor {
         *     bool do_act() noexcept { ... }
         * public:
         *     MyActor() : Actor<MyActor>(&ActorThread, &MyActor::do_act) { ... }
         * };
         * @endcode
         */
        template <class ActorType>
        Actor(bool (ActorType::*act_function)() noexcept) noexcept
            : Actor(reinterpret_cast<bool (Actor::*)() noexcept>(act_function)) {
            static_assert(std::is_base_of<Actor, ActorType>::value, "implementation must be inherited from Actor class");
        }

        /// virtual destructor
        virtual ~Actor();

        // disallow copying
        Actor(const Actor &) = delete;
        Actor & operator= (const Actor &) = delete;
        // allow moving
        Actor(Actor &&) = default;
        Actor & operator=(Actor &&) = default;

        /// start to execute thread
        void start() noexcept;

        /// request thread to stop execution
        void stop() noexcept;

        /// wait for thread completion
        void join() noexcept;

        /// check whether actor was interrupted
        /// @return value is undefined upon start
        bool interrupted() noexcept;

    protected:
        /// Send message to the other actor
        template <typename ... Args>
        void send_to(std::shared_ptr<Actor> to, atom_type msg_type, Args&&... args) noexcept;

        /// Reply to the received message
        /// @return `true` if message sent, `false` if recepient is dead
        template <typename ... Args>
        bool reply_to(UniqueMessagePtr && msg, atom_type msg_type, Args ... args) noexcept;

        /// Receive message
        UniqueMessagePtr receive_message() noexcept;

        /// wait a few cycles
        void pause() noexcept;

    private:
        /// notify Thread to wake it up
        void notify() noexcept;

    private:
        typedef mpsc_queue<Message> Mailbox;
        Mailbox m_mailbox;
        class Thread; // separate thread implementation
        std::unique_ptr<Thread> m_thread;
        std::shared_ptr<Actor> __me; // satisfy precondition for a weak_ptr
    };


///////////////// Message implementation ///////////////////////////////////////////////////

    /**
     * MessageBase holds fixed set of arguments and represents node of
     * intrusive Mailbox queue.
     * MessageBase is internal class of Actor
     */
    class Actor::MessageBase : private Actor::Mailbox::node {
    public:
        /// sender Actor of this message
        std::weak_ptr<Actor> sender;

        /// type of message
        const atom_type type;

    protected:
        /// constructor
        explicit MessageBase(Actor & the_sender, const atom_type the_type) noexcept
            : sender(the_sender.shared_from_this())
            , type(the_type) {
        }

    private:
        // disallow copying
        MessageBase(const MessageBase &) = delete;
        MessageBase & operator=(const MessageBase &) = delete;
        // disallow moving
        MessageBase(MessageBase &&) = delete;
        MessageBase & operator=(const MessageBase &&) = delete;
    };

    
    /**
     * Message used to pass arbitrary arguments between threads
     *
     * @see Actor::send_message() Actor::on_message
     *
     *  TODO: type check (array of type_infos of message argument)
     */
    class Actor::Message : public Actor::MessageBase {
    private:
        // every message has fixed size, that allows to use pool for memory management
        static constexpr size_t message_size = 128;
        // messages pool maximal size
        static constexpr size_t message_pool_limit = 256;

        // disallow copying
        Message(const Message &) = delete;
        Message & operator= (const Message &) = delete;

        /// private constructor
        template <typename ...  Args>
        explicit Message(Actor & the_sender, const atom_type the_type, Args ... args) noexcept
            : Actor::MessageBase(the_sender, the_type) {
            static_assert(std::is_trivially_destructible<tuple<Args...>>::value, "Arguments must be trivially destructible");
            static_assert(sizeof(Message) == message_size, "Unexpected message size");
            static_assert(sizeof(tuple<Args...>) < payload_size, "Not enough space for the arguments");
            // ensure that payload memory is properly aligned
            debug_assert(unaligned_bytes(payload, alignof(tuple<Args...>)) == 0);
            // assign arguments
            new (payload) tuple<Args...>(args ...);
        }

        /// private destructor
        ~Message() = default;

    public:

        /**
         * unpack message arguments
         */
        template<typename ... Args>
        void unpack(Args & ... args) const noexcept {
            std::tie<Args...>(args...) = *reinterpret_cast<const tuple<Args...> *>(payload);
        }

    private:
        static constexpr size_t payload_size = message_size - sizeof(Actor::MessageBase);
        uint8 payload[payload_size];  // payload memory to hold arguments

    private:
        friend Actor;
        friend std::default_delete<Message>;
    private:
        friend test_actor::test_message_arguments;
    };


///////////////// Actor implementation ///////////////////////////////////////////////////


    template <typename ... Args>
    inline void Actor::send_to(std::shared_ptr<Actor> to, atom_type msg_type, Args && ... args) noexcept {
        if (to) {
            to->m_mailbox.enqueue(new Actor::Message(*this, msg_type, std::forward<Args>(args) ...));
            to->notify();
        }
    }


    template <typename ... Args>
    inline bool Actor::reply_to(UniqueMessagePtr && msg, atom_type msg_type, Args ... args) noexcept {
        debug_assert(msg);
        auto to = msg->sender.lock();
        if (to) {
            // re-create Message inplace unique_ptr::release call destructor
            Actor::Message * reply_msg = new (msg.release()) Actor::Message(*this, msg_type, args ...);
            to->m_mailbox.enqueue(reply_msg);
            to->notify();
            return true;
        }
        return false;
    }


    inline Actor::UniqueMessagePtr Actor::receive_message() noexcept {
        return std::move(UniqueMessagePtr(m_mailbox.dequeue()));
    }

} // namespace cachelot

/// @}

#endif // CACHELOT_ACTOR_H_INCLUDED

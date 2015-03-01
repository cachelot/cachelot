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

    class ActorThread {
        class ActorThreadImpl;
    public:
        typedef std::function<bool ()> MainFunction;

        /// constructor
        ActorThread(MainFunction func);

        /// destructor
        ~ActorThread();

        /// start to execute thread
        void start() noexcept;

        /// request thread to stop execution
        void stop() noexcept;

        /// wait for thread completion
        void join() noexcept;

    private:
        std::shared_ptr<ActorThreadImpl> m_impl;
        friend class Actor;
    };


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
    class Actor {
        class MessageBase;
    public:
        class Message;

        typedef std::weak_ptr<ActorThread> WeakThreadPtr;

        typedef std::function<bool (Actor *) noexcept> actor_body;

        ///@name constructors
        ///@{

        /// default constructor
        Actor() noexcept;

        /**
         * @tparam ActorType class of specific actor implementation
         * @code
         * class MyActor : Actor {
         *     Actor::Reply handle_message(Actor::Message & msg) noexcept { ... }
         *     bool main() noexcept { ... }
         * public:
         *     MyActor() : Actor<MyActor>(&MyActor::handle_message, &MyActor::main) { ... }
         * };
         * @endcode
         */
        template <class ActorType>
        Actor(ActorThread & execution_thread, bool (ActorType::*act_function)() noexcept) noexcept
            : m_execution_thread(execution_thread.m_impl)
            , do_act(reinterpret_cast<bool (Actor::*)() noexcept>(act_function)) {
            static_assert(std::is_base_of<Actor, ActorType>::value, "implementation must be inherited from Actor class");
            attach();
        }
        ///@} // ^constructors

        /// virtual destructor
        virtual ~Actor() {
            detach();
        }

        // disallow copying
        Actor(const Actor &) = delete;
        Actor & operator= (const Actor &) = delete;
        // allow moving
        Actor(Actor &&) = default;
        Actor & operator=(Actor &&) = default;

        bool act() noexcept { return do_act(this); }

    protected:
        /// Send message to the other actor
        template <typename ... Args>
        void send_message(Actor & to, atom_type msg_type, Args&&... args) noexcept;

        /// Reply to the received message
        template <typename ... Args>
        void reply_to(std::unique_ptr<Message> && msg, atom_type msg_type, Args ... args) noexcept;

        /// Receive message
        std::unique_ptr<Message> receive_message() noexcept;

    private:
        /// notify owner ActorThread to wake it up
        void notify() noexcept;

        /// attach this actor to the thread
        void attach() noexcept;

        /// detach this actor from the thread
        void detach() noexcept;

    private:
        typedef mpsc_queue<Message> Mailbox;
        mpsc_queue<Message> m_mailbox;
        std::weak_ptr<ActorThread::ActorThreadImpl> m_execution_thread;
        actor_body do_act;

    private:
        friend class ActorThread;
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
        Actor & sender;

        /// type of message
        const atom_type type;

    protected:
        /// constructor
        explicit MessageBase(Actor & the_sender, const atom_type the_type) noexcept
            : sender(the_sender)
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
    inline void Actor::send_message(Actor & to, atom_type msg_type, Args && ... args) noexcept {
        to.m_mailbox.enqueue(new Actor::Message(*this, msg_type, std::forward<Args>(args) ...));
        to.notify();
    }


    template <typename ... Args>
    inline void Actor::reply_to(std::unique_ptr<Message> && msg, atom_type msg_type, Args ... args) noexcept {
        debug_assert(msg);
        Actor & to = msg->sender;
        Actor::Message * reply_msg = new (msg.release()) Actor::Message(*this, msg_type, args ...);
        to.m_mailbox.enqueue(reply_msg);
        to.notify();
    }

    inline std::unique_ptr<Actor::Message> Actor::receive_message() noexcept {
        return std::unique_ptr<Actor::Message>(m_mailbox.dequeue());
    }

} // namespace cachelot

/// @}

#endif // CACHELOT_ACTOR_H_INCLUDED

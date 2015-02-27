#ifndef CACHELOT_ACTOR_H_INCLUDED
#define CACHELOT_ACTOR_H_INCLUDED

#include <cachelot/atom.h>
#include <cachelot/bits.h>  // unaligned_bytes
#include <cachelot/pool_allocator.h>
#include <cachelot/mpsc_queue.h>
#include <thread>
#include <atomic>

namespace cachelot {

    /**
     * Actor is an interface for implementing actors that are running in separate threads
     * and communicate with each other via messages.
     * (It's inspired by Erlang actor model although there's lack of major functionality, like failover handling)
     *
     * Specific actor implementation class must be inherited from Actor and provide 
     * callbacks: one for periodical executing in main loop and one for incoming messages
     * @par Example
     * @snippet src/unit_test/test_actor.cpp Simple calculator actors
     * @see Message
     * @see on_message and on_idle
     * @note It is possible to implement state machine by changing `on_message` callback
     */
    class Actor {
        class MessageBase;
    public:
        /**
         * Reply is internally used to indicate whether message is replied or not
         * @see reply_to() and noreply()
         */
        class Reply {
            const bool flag = false;
            explicit Reply(bool f) : flag(f) {}
            explicit operator bool() { return flag; }
            friend class Actor;
        };

        class Message;

        typedef std::function<Reply (Actor *, Message &) noexcept> message_callback_type;
        typedef std::function<bool (Actor *) noexcept> idle_callback_type;


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
        Actor(Actor::Reply (ActorType::*on_message_fun)(Actor::Message &) noexcept,
                bool (ActorType::*on_idle_fun)() noexcept) noexcept
            : on_message(reinterpret_cast<Actor::Reply (Actor::*)(Actor::Message &) noexcept>(on_message_fun))
            , on_idle(reinterpret_cast<bool (Actor::*)() noexcept>(on_idle_fun)) {
            static_assert(std::is_base_of<Actor, ActorType>::value, "implementation must be inherited from Actor class");
            }
        ///@} // ^constructors

        /// virtual destructor
        virtual ~Actor() {}

        // disallow copying
        Actor(const Actor &) = delete;
        Actor & operator= (const Actor &) = delete;

        /// start actor thread (can not be called for running actor)
        void start();

        /// request actor to interrupt (can be safely called from actor callbacks)
        void stop() noexcept;

        /** 
         * blocks caller thread until this actor finishes execution
         * @note calling this function from actor callbacks will cause deadlock
         */
        void wait_complete() noexcept;

    protected:
        /// Send message to the other actor
        template <typename ... Args>
        void send_message(Actor & to, atom_type msg_type, Args&&... args);

        /// Reply to the received message
        template <typename ... Args>
        Reply reply_to(Message & msg, atom_type msg_type, Args ... args);

        /// indicate that message is left unreplied
        Reply noreply() const noexcept { return Reply(false); }

        /** 
         * @name callbacks
         * Every Actor implementations must assign two following callbacks:
         *@{
         */

        /**
         * callback to handle incoming messages from other actors
         * @code
         * Reply on_message(Actor * self, Message & msg) noexcept
         * @endcode
         * function must indicate whether message was replied or not
         * by calling either Actor::reply_to() or Actor::noreply()
         */
        message_callback_type on_message;
        /**
         * main actor function that will be periodically executed in a loop
         * @code
         * bool on_idle(Actor * self) noexcept
         * @endcode
         * function must return `true` if it does something useful
         * and `false` if it has nothing to do
         */
        idle_callback_type on_idle;
        ///@} // ^callbacks

    public:
        bool do_nothing() noexcept { return false; }

    private:
        typedef mpsc_queue<Message> Mailbox;

        std::thread m_thread;
        mpsc_queue<Message> m_mailbox;
        std::atomic_bool m_is_interrupted;  // flag to stop thread execution
        chrono::nanoseconds __time_to_sleep; // used by yield implementation
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

        /// opaque user data will not be modified when the message will be replied
        void * const userdata;

    protected:
        /// constructor
        explicit MessageBase(Actor & the_sender, const atom_type the_type, void * the_userdata) noexcept
            : sender(the_sender)
            , type(the_type)
            , userdata(the_userdata) {
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
        explicit Message(Actor & the_sender, const atom_type the_type, void * the_userdata, Args ... args) noexcept
            : Actor::MessageBase(the_sender, the_type, the_userdata) {
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
        /// Create new message
        template <typename ...Args>
        static Message * Create(Actor & the_sender, const atom_type the_type, void * the_userdata, Args ... args) {
            Message * msg = LazyPoolInit().create(the_sender, the_type, the_userdata, args ...);
            debug_assert(msg != nullptr);
            return msg;
        }

        /// Destroy the message
        static void Dispose(Message * msg) noexcept {
            debug_assert(msg != nullptr);
            LazyPoolInit().destroy(msg);
        }

        /**
         * unpack message arguments
         */
        template<typename ... Args>
        void unpack(Args & ... args) const noexcept {
            std::tie<Args...>(args...) = *reinterpret_cast<const tuple<Args...> *>(payload);
        }

    private:
        static constexpr size_t payload_size = message_size - sizeof(Actor::MessageBase);
        byte payload[payload_size];  // payload memory to hold arguments

    private:
        typedef pool_allocator<Message, message_pool_limit> message_pool_type;
        friend message_pool_type;
        friend Actor;
        static message_pool_type & LazyPoolInit() {
            static message_pool_type instance;
            return instance;
        }
    };


///////////////// Actor implementation ///////////////////////////////////////////////////

    template <typename ... Args>
    inline void Actor::send_message(Actor & to, atom_type msg_type, Args && ... args) {
        to.m_mailbox.enqueue(Actor::Message::Create(*this, msg_type, std::forward<Args>(args) ...));
    }


    template <typename ... Args>
    inline Actor::Reply Actor::reply_to(Message & msg, atom_type msg_type, Args ... args) {
        Actor & to = msg.sender;
        auto userdata = msg.userdata;
        Actor::Message * reply_msg = new (&msg) Actor::Message(*this, msg_type, userdata, args ...);
        to.m_mailbox.enqueue(reply_msg);
        return Actor::Reply(true);
    }

} // namespace cachelot

#endif // CACHELOT_ACTOR_H_INCLUDED

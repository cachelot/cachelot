#ifndef CACHELOT_PROTO_MEMCACHED_TEXT
#define CACHELOT_PROTO_MEMCACHED_TEXT

#ifndef CACHELOT_PROTO_MEMCACHED_H_INCLUDED
#  include <cachelot/proto_memcached.h>
#endif
#ifndef CACHELOT_ACTOR_H_INCLUDED
#  include <cachelot/actor.h>
#endif

/**
 * @ingroup memcached
 * @{
 */

namespace cachelot {

    namespace memcached {

    using namespace cachelot::net;

    constexpr bytes CRLF = bytes::from_literal("\r\n");
    constexpr char SPACE = ' ';

    template <class SocketType>
    class text_protocol_handler : public async_connection<SocketType, text_protocol_handler<SocketType>>, public Actor {
        typedef async_connection<SocketType, text_protocol_handler<SocketType>> parent_connection;
        typedef text_protocol_handler<SocketType> this_type;
        typedef io_stream<text_serialization_tag> ostream_type;

        static const size_t command_max_length = 7;

        /// constructor
        explicit text_protocol_handler(io_service & io_svc, std::shared_ptr<Actor> the_cache)
            : parent_connection(io_svc)
            , Actor(&this_type::main)
            , m_ios(io_svc)
            , cache(the_cache)
            , calc_hash() {
        }

        /// destructor
        ~text_protocol_handler() {}

    public:
        /// create new connection
        static this_type * create(io_service & io_svc, std::shared_ptr<Actor> the_cache) {
            return new this_type(io_svc, the_cache);
        }

        static void destroy(this_type * this_) noexcept {
            debug_assert(this_);
            delete this_;
        }

        void run() noexcept {
            Actor::start();
            net_receive();
        }

    private:
        /// main thread function
        bool main() noexcept;

        /// handle reply message from the cache service
        void on_cache_reply(Actor::UniqueMessagePtr && msg) noexcept;

        /// wait until cache handle request
        void wait_for_cache_reply() noexcept;

        /// start to receive new request
        void net_receive() noexcept;

        /// request received handler
        void on_net_receive(const bytes buffer) noexcept;

        /// error handler
        void on_error(const error_code error) noexcept;

        /// send erroneous response to the client
        void send_error(const ErrorType errtype, const bytes error_msg) noexcept;

        /// send standard response to the client
        void send_response(const Response res) noexcept;

        /// add single item to the send buffer
        void push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::cas_value_type cas_value) noexcept;

        ostream_type serialize() noexcept { return ostream_type(this->send_buffer()); }

        /// close this connection and move it to the pool
        void suicide() noexcept;

        /// try to send all unsent data in buffer
        void flush() noexcept;

        /// convert name of the command into atom
        atom_type parse_command_name(const bytes command) noexcept;

        void handle_storage_request(atom_type req, bytes args_buf);
        void handle_delete_request(atom_type req, bytes args_buf);
        void handle_arithmetic_request(atom_type req, bytes args_buf);
        void handle_touch_request(atom_type req, bytes args_buf);
        void handle_retrieval_request(atom_type req, bytes args_buf);
        void handle_service_request(atom_type req, bytes args_buf);

        void validate_key(const bytes key);
        bool maybe_noreply(const bytes buffer);

    private:
        io_service & m_ios;
        std::shared_ptr<Actor> cache;
        const HashFunction calc_hash;
        bool m_killed = false; // TODO: Move error handling to async_connection???
    };


    template <class Sock>
    inline bool text_protocol_handler<Sock>::main() noexcept {
        error_code error;
        bool has_work = m_ios.poll(error) > 0;
        if (error) {
            on_error(error);
        }
        auto msg = receive_message();
        if (msg) {
            has_work = true;
            on_cache_reply(std::move(msg));
        }
        return has_work;
    }



    template <class Sock>
    inline void text_protocol_handler<Sock>::on_cache_reply(Actor::UniqueMessagePtr && msg) noexcept {
        switch (msg->type) {
            case atom("stored"):
                send_response(memcached::STORED);
                break;
            case atom("not_stored"): {
                send_response(memcached::NOT_STORED);
                break;
            }
            case atom("flush"): {
                flush();
                break;
            }
            case atom("error"): {
                error_code err;
                msg->unpack(err);
                send_error(memcached::SERVER_ERROR, bytes(err.message().c_str(), err.message().size()));
            }
            case atom("die"): {
                this->close();
                io_service().post([=](){
                    this->stop();
                    this->join();
                });
            }
            default:
                debug_assert(false && "Error - unknown message");
                break;
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::wait_for_cache_reply() noexcept {
        Actor::UniqueMessagePtr msg;
        while (not msg) {
            msg = receive_message();
            if (msg) {
                on_cache_reply(std::move(msg));
            } else {
                pause();
            }
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::net_receive() noexcept {
        this->async_receive_until(CRLF,
            [=](const error_code error, const bytes data) {
                if (!error) {
                    debug_assert(data.endswith(CRLF));
                    this->on_net_receive(data);
                } else {
                    this->on_error(error);
                }
        });
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_error(const ErrorType etype, const bytes error_msg) noexcept {
        this->serialize() << AsciiErrorType(etype) << ' ' << error_msg << CRLF;
        net_debug_mtrace("<- [error] [%s: %s]", AsciiErrorType(etype).str().c_str(), error_msg.str().c_str());
        this->flush();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_response(const Response res) noexcept {
        this->serialize() << AsciiResponse(res) << CRLF;
        net_debug_mtrace("<- [response] [%s]", AsciiResponse(res).str().c_str());
        this->flush();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::on_net_receive(const bytes buffer) noexcept {
        try {
            bytes command_name, command_args, __;
            tie(command_name, command_args) = buffer.split(SPACE);
            if (command_args) {
                command_args = command_args.rtrim_n(CRLF.length());
            } else {
                command_name = command_name.rtrim_n(CRLF.length());
            }
            net_debug_mtrace("->[command: %s] [args: %s]", command_name.str().c_str(), command_args.str().c_str());
            bool receive_next_command = true;
            atom_type req_type = parse_command_name(command_name);
            switch (req_type) {
                case atom("get"):
                case atom("gets"):
                    handle_retrieval_request(req_type, command_args);
                    break;
                case atom("set"):
                case atom("add"):
                case atom("replace"):
                case atom("cas"):
                    // handle_storage_request has to read data first and then
                    // will call net_receive() by itself
                    receive_next_command = false;
                    handle_storage_request(req_type, command_args);
                    break;
                case atom("del"):
                    break;
                case atom("int"):
                case atom("dec"):
                    //handle_arithmetic_request(command_args);
                    break;
                case atom("touch"):
                    //handle_touch_request(command_args);
                    break;
                case atom("quit"):
                    suicide();
                    break;
                default:
                    throw non_existing_request_error();
            }
            if (receive_next_command) {
                net_receive();
            }
            return;
        } catch(const memcached_error & err) {
            //this->flush(););
            // TODO: !!!!
        } catch(const std::exception & err) {
            //this->flush(););
            // TODO: !!!!
        } catch(...) {
            // TODO: !!!!
        }
        // if we reach here ...
        suicide();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_request(atom_type req, bytes arguments_buf) {
        // command arguments
        bytes key;
        tie(key, arguments_buf) = arguments_buf.split(SPACE);
        validate_key(key);
        bytes parsed;
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        cache::opaque_flags_type flags = str_to_int<cache::opaque_flags_type>(parsed.begin(), parsed.length()); // TODO: flags must feed 16bit
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        auto expires_after = cache::seconds(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.length()));
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        uint32 datalen = str_to_int<uint32>(parsed.begin(), parsed.length());
        if (datalen > settings.cache.max_value_size) {
            throw client_error("Maximum value length exceeded");
        }
        cache::cas_value_type cas_unique = 0;
        if (req == atom("cas")) {
            tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
            cas_unique = str_to_int<cache::cas_value_type>(parsed.begin(), parsed.length());
        }
        bool noreply = maybe_noreply(arguments_buf);
        if (not arguments_buf.empty()) {
            throw client_error("invalid command: \\r\\n expected");
        }
        // read value
        this->async_receive_n(datalen + CRLF.length(),
            [=](const error_code net_error, const bytes data) noexcept {
                // at this point we have +1 reference from intrusive_ptr capturing in lambda context
                if (not net_error) {
                    bytes value;
                    if (data.endswith(CRLF)) {
                        value = data.rtrim_n(CRLF.length());
                    } else {
                        send_error(CLIENT_ERROR, bytes::from_literal("invalid value: \\r\\n expected"));
                        suicide();
                    }
                    send_to(cache, req, key, calc_hash(key), value, flags, expires_after, cas_unique, noreply);
                    net_receive();
                    wait_for_cache_reply();
                } else {
                    on_error(net_error);
                }
            });
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_retrieval_request(atom_type req, bytes args_buf) {
        do {
            bytes key;
            tie(key, args_buf) = args_buf.split(SPACE);
            validate_key(key);
            send_to(cache, req, key, calc_hash(key), atom("text"), &this->send_buffer());
            wait_for_cache_reply();
        } while (args_buf);
        send_to(cache, atom("sync"), atom("flush"));
        wait_for_cache_reply();
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_service_request(atom_type req, bytes args_buf) {
        (void)args_buf;
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::validate_key(const bytes key) {
        if (not key) {
            throw client_error("The key is empty");
        }
        if (key.length() > cache::Item::max_key_length) {
            throw client_error("The key is too long");
        }
    }


    template <class Sock>
    inline bool text_protocol_handler<Sock>::maybe_noreply(const bytes noreply) {
        static const char NOREPLY_[] = "noreply";
        static const bytes NOREPLY = bytes(NOREPLY_, sizeof(NOREPLY_) - 1);
        if (noreply.empty()) {
            return false;
        } else if (noreply == NOREPLY) {
            return true;
        } else {
            throw client_error("invalid command arguments: [noreply] expected");
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::on_error(const error_code error) noexcept {
        debug_assert(error);
        suicide();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::suicide() noexcept {
        // 'suicide' can be called several times from background operation handlers
        // we must ensure that it'll be only one attempt to delete this object
        Actor::stop();
        if (! m_killed) {
            m_killed = true;
            send_to(cache, atom("sync"), atom("die"));
        }
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::flush() noexcept {
        this->async_send_all([=](const error_code error) {
                            if (error) { this->on_error(error); }
                        });
    }

    template <class Sock>
    inline atom_type text_protocol_handler<Sock>::parse_command_name(const bytes command) noexcept {
        if (not command) {
            return atom("undefined");
        }
        auto is_ = [=](const char * literal) -> bool {
            return std::strncmp(command.begin(), literal, command.length()) == 0;
        };
        const char first_char = command[0];
        switch (command.length()) {
            case 3:
                switch (first_char) {
                    case 'a': return is_("add") ? atom("add") : atom("undefined");
                    case 'c': return is_("cas") ? atom("cas") : atom("undefined");
                    case 'g': return is_("get") ? atom("get") : atom("undefined");
                    case 's': return is_("set") ? atom("set") : atom("undefined");
                    default : return atom("undefined");
                }
            case 4:
                switch (first_char) {
                    case 'd': return is_("decr") ? atom("decr") : atom("undefined");
                    case 'g': return is_("gets") ? atom("gets") : atom("undefined");
                    case 'i': return is_("incr") ? atom("incr") : atom("undefined");
                    case 'q': return is_("quit") ? atom("quit") : atom("undefined");
                    default : return atom("undefined");
                }
            case 5:
                return is_("touch") ? atom("touch") : atom("undefined");
            case 6:
                switch (first_char) {
                    case 'a': return is_("append") ? atom("append") : atom("undefined");
                    case 'd': return is_("delete") ? atom("delete") : atom("undefined");
                    default : return atom("undefined");
                }
            case 7:
                switch (first_char) {
                    case 'p': return is_("prepend") ? atom("prepend") : atom("undefined");
                    case 'r': return is_("replace") ? atom("replace") : atom("undefined");
                    default : return atom("undefined");
                }
            default :
                return atom("undefined");
        }
    }

} } // namespace namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_TEXT

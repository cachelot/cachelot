#ifndef CACHELOT_PROTO_MEMCACHED_TEXT
#define CACHELOT_PROTO_MEMCACHED_TEXT

#include <cachelot/async_connection.h>
#include <cachelot/io_buffer.h>
#include <cachelot/io_serialization.h>

/**
 * @defgroup memcached Memcached protocol implementation
 * @{
 */

namespace cachelot {

    namespace memcached {

    using namespace cachelot::net;

    extern const bytes CRLF;
    extern const char SPACE;

    template <class SocketType>
    class text_protocol_handler : public async_connection<SocketType, text_protocol_handler<SocketType>>, public slist_link {
        typedef async_connection<SocketType, text_protocol_handler<SocketType>> super;
        typedef text_protocol_handler<SocketType> this_type;
        typedef io_stream<text_serialization_tag> ostream_type;

        static const size_t command_max_length = 7;

        /// constructor
        explicit text_protocol_handler(io_service & io_svc, cache_container & the_cache)
            : super(io_svc)
            , cache(the_cache) {

        }

        /// destructor
        ~text_protocol_handler() {}
    public:
        /// create new connection
        static this_type * create(io_service & io_svc, cache_container & the_cache) {
            //this_type * new_connection = connection_pool.get();
            //if (!new_connection)
                return new this_type(io_svc, the_cache);
        }

        static void destroy(this_type * this_) noexcept {
            debug_assert(this_);
            delete this_;
        }

        void run() noexcept { receive_command(); }

    private:
        /// close this connection and move it to the pool
        void suicide() noexcept;

        void receive_command() noexcept;

        void on_command(const bytes buffer) noexcept;

        void on_error(const error_code error) noexcept;

        void send_error(const api::ErrorType errtype, const bytes error_msg) noexcept;

        void send_response(const api::Response res) noexcept;

        void push_item(cache_item_ptr it, bool send_cas) noexcept;

        ostream_type serialize() noexcept { return ostream_type(this->send_buffer()); }

        api::command determine_command(const bytes command);
        void handle_storage_command(api::command cmd, bytes args_buf);
        void handle_delete_command(api::command cmd, bytes args_buf);
        void handle_arithmetic_command(api::command cmd, bytes args_buf);
        void handle_touch_command(api::command cmd, bytes args_buf);
        void handle_retrieval_command(api::command cmd, bytes args_buf);
        void handle_service_command(api::command cmd, bytes args_buf);

        void validate_key(const bytes key);
        bool maybe_noreply(const bytes buffer);
/*
    private:
        class connection_pool_type {
        public:
            connection_pool_type();
            ~connection_pool_type();
            this_type * get();
            void put(this_type *);
        private:
            boost::intrusive::slist<this_type> m_freelist;
        };
        static connection_pool_type & connection_pool() {
            static connection_pool_type the_pool;
            return the_pool;
        }
 // */
    private:
        cache_container & cache;
        bool m_killed = false; // TODO: Move error handling to async_connection???
    };


    template <class Sock>
    inline void text_protocol_handler<Sock>::receive_command() noexcept {
        this->async_receive_until(CRLF,
            [=](const error_code error, const bytes data) {
                if (!error) {
                    debug_assert(data.endswith(CRLF));
                    this->on_command(data);
                } else {
                    this->on_error(error);
                }
        });
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_error(const api::ErrorType etype, const bytes error_msg) noexcept {
        this->serialize() << api::AsciiErrorType(etype) << ' ' << error_msg << CRLF;
        net_debug_mtrace("<- [error] [%s: %s]", api::AsciiErrorType(etype).str().c_str(), error_msg.str().c_str());
        this->async_send_all(
            [=](const error_code error) {
                if (error) {
                    on_error(error);
                }
            });
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_response(const api::Response res) noexcept {
        this->serialize() << api::AsciiResponse(res) << CRLF;
        net_debug_mtrace("<- [response] [%s]", api::AsciiResponse(res).str().c_str());
        this->async_send_all(
            [=](const error_code error) {
                if (error) {
                    on_error(error);
                }
            });
    }


    template <class Sock>
    void text_protocol_handler<Sock>::push_item(cache_item_ptr i, bool send_cas) noexcept {
        const auto value_length = static_cast<unsigned>(i->value().length());
        static const bytes VALUE = bytes::from_literal("VALUE ");
        this->serialize() << VALUE << i->key() << ' ' << i->user_flags() << ' ' << value_length;
        if (send_cas) {
            this->serialize() << ' ' << i->cas_unique();
        }
        this->serialize() << CRLF << i->value() << CRLF;
    }



    template <class Sock>
    inline void text_protocol_handler<Sock>::on_command(const bytes buffer) noexcept {
        try {
            bytes command_name, command_args, __;
            tie(command_name, command_args) = buffer.split(SPACE);
            if (command_args) {
                command_args = command_args.rtrim_n(CRLF.length());
            } else {
                command_name = command_name.rtrim_n(CRLF.length());
            }
            net_debug_mtrace("->[command: %s] [args: %s]", command_name.str().c_str(), command_args.str().c_str());
            api::command cmd = determine_command(command_name);
            api::command_class cmd_class = api::get_command_class(cmd);
            switch (cmd_class) {
                case api::storage_command:
                    handle_storage_command(cmd, command_args);
                    break;
                case api::delete_command:
                    break;
                case api::arithmetic_command:
                    //handle_arithmetic_command(command_args);
                    break;
                case api::touch_command:
                    //handle_touch_command(command_args);
                    break;
                case api::retrieval_command:
                    handle_retrieval_command(cmd, command_args);
                    break;
                case api::service_command:
                    handle_service_command(cmd, command_args);
                case api::quit_command:
                    suicide();
                    break;
                default:
                    throw non_existing_command_error();
            }
            if (cmd_class != api::storage_command) {
                // handle_storage_command has to read data first and then
                // will call receive_command() by itself
                receive_command();
            }
            return;
        } catch(const memcached_error & err) {
            //this->async_send_all();
            // TODO: !!!!
        } catch(const std::exception & err) {
            //this->async_send_all();
            // TODO: !!!!
        } catch(...) {
            // TODO: !!!!
        }
        // if we reach here ...
        suicide();
    }


    template <class Sock>
    inline api::command text_protocol_handler<Sock>::determine_command(const bytes command) {
        if (not command) {
            return api::UNDEFINED;
        }
        auto is_ = [=](const char * literal) -> bool {
            return std::strncmp(command.begin(), literal, command.length()) == 0;
        };
        const char first_char = command[0];
        switch (command.length()) {
        case 3:
            switch (first_char) {
            case 'a': return is_("add") ? api::ADD : api::UNDEFINED;
            case 'c': return is_("cas") ? api::CAS : api::UNDEFINED;
            case 'g': return is_("get") ? api::GET : api::UNDEFINED;
            case 's': return is_("set") ? api::SET : api::UNDEFINED;
            default : return api::UNDEFINED;
            }
        case 4:
            switch (first_char) {
            case 'd': return is_("decr") ? api::DECR : api::UNDEFINED;
            case 'g': return is_("gets") ? api::GETS : api::UNDEFINED;
            case 'i': return is_("incr") ? api::INCR : api::UNDEFINED;
            case 'q': return is_("quit") ? api::QUIT : api::UNDEFINED;
            default : return api::UNDEFINED;
            }
        case 5:
            return is_("touch") ? api::TOUCH : api::UNDEFINED;
        case 6:
            switch (first_char) {
            case 'a': return is_("append") ? api::APPEND : api::UNDEFINED;
            case 'd': return is_("delete") ? api::DELETE : api::UNDEFINED;
            default : return api::UNDEFINED;
            }
        case 7:
            switch (first_char) {
            case 'p': return is_("prepend") ? api::PREPEND : api::UNDEFINED;
            case 'r': return is_("replace") ? api::REPLACE : api::UNDEFINED;
            default : return api::UNDEFINED;
            }
        default : return api::UNDEFINED;
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_command(api::command cmd, bytes arguments_buf) {
        // command arguments
        bytes key;
        tie(key, arguments_buf) = arguments_buf.split(SPACE);
        validate_key(key);
        bytes parsed;
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        user_flags_t flags = str_to_long(parsed.begin(), parsed.length()); // TODO: flags must feed 16bit
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        auto expires_after = chrono::seconds(str_to_u_long(parsed.begin(), parsed.length()));
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        uint32 datalen = str_to_u_long(parsed.begin(), parsed.length());
        cas_t cas_unique = 0;
        if (cmd == api::CAS) {
            tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
            cas_unique = str_to_longlong(parsed.begin(), parsed.length());
        }
        bool noreply = maybe_noreply(arguments_buf);
        if (not arguments_buf.empty()) {
            throw client_error("invalid command: \\r\\n expected");
        }
        cache_item_ptr new_item = cache_item::create(key, flags, expires_after, cas_unique);
        new_item->retain(); // async_receive_n handler lambda will release it (see below)
        // read value
        this->async_receive_n(datalen + CRLF.length(),
            [=](const error_code error, const bytes data) noexcept {
                // at this point we have +1 reference from intrusive_ptr capturing in lambda context
                new_item->release();
                if (!error) {
                    bytes value;
                    if (data.endswith(CRLF)) {
                        new_item->assign_value(data.rtrim_n(CRLF.length()));
                    } else {
                        send_error(api::CLIENT_ERROR, bytes::from_literal("invalid value: \\r\\n expected"));
                        suicide();
                    }
                    api::Response response = api::NOT_A_RESPONSE;
                    switch (cmd) {
                    case api::ADD:
                        response = cache.add(new_item);
                        break;
                    case api::APPEND:
                        response = cache.append(new_item);
                        break;
                    case api::CAS:
                        response = cache.cas(new_item);
                    case api::PREPEND:
                        response = cache.prepend(new_item);
                        break;
                    case api::REPLACE:
                        response = cache.replace(new_item);
                        break;
                    case api::SET:
                        response = cache.set(new_item);
                        break;
                    default:
                        debug_assert(false && "This shouldn't happen");
                        break;
                    }
                    if (not noreply) {
                        send_response(response);
                    }
                    receive_command();
                } else {
                    on_error(error);
                }
            });
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_retrieval_command(api::command cmd, bytes args_buf) {
        const bool send_cas = cmd == api::GETS;
        do {
            bytes key;
            tie(key, args_buf) = args_buf.split(SPACE);
            validate_key(key);
            cache_item_ptr item = cache.get(key);
            if (item) {
                debug_mtrace("<-[get] [%s]", key.str().c_str());
                push_item(item, send_cas);
            }
        } while (args_buf);
        static const bytes END = bytes::from_literal("END");
        this->serialize() << END << CRLF;
        this->async_send_all(
            [=](const error_code error) {
                if (error) {
                    this->on_error(error);
                }
            });
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_service_command(api::command cmd, bytes args_buf) {

    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::validate_key(const bytes key) {
        if (not key) {
            throw client_error("The key is empty");
        }
        if (key.length() > MaxKeyLength) {
            throw client_error("The key is too long");
        }
    }

    template <class Sock>
    inline bool text_protocol_handler<Sock>::maybe_noreply(const bytes noreply) {
        static const char NOREPLY_[] = "noreply";
        static const bytes NOREPLY = bytes(NOREPLY_, sizeof(NOREPLY_) - 1);
        if (noreply.empty()) {
            return false;
        } else if (noreply.equals(NOREPLY)) {
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
        if (! m_killed) {
            m_killed = true;
            this->close();
            io_service().post([=](){ delete this; });
        }
    }

} } // namespace namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_TEXT

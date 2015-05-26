#ifndef CACHELOT_PROTO_MEMCACHED_TEXT
#define CACHELOT_PROTO_MEMCACHED_TEXT

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <server/memcached/memcached.h>

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
    class text_protocol_handler : public async_connection<SocketType, text_protocol_handler<SocketType>> {
        typedef async_connection<SocketType, text_protocol_handler<SocketType>> super;
        typedef text_protocol_handler<SocketType> this_type;
        typedef io_stream<text_serialization_tag> ostream_type;

        static const size_t command_max_length = 7;

        /// constructor
        explicit text_protocol_handler(io_service & io_svc, cache::Cache & the_cache)
            : super(io_svc)
            , cache(the_cache)
            , calc_hash()
            , m_killed(false) {
        }

        /// destructor
        ~text_protocol_handler() {}
    public:
        /// create new connection
        static this_type * create(io_service & io_svc, cache::Cache & the_cache) {
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

        /// try to send all unsent data in buffer
        void flush() noexcept;

        /// start to receive new command
        void receive_command() noexcept;

        /// command received handler
        void on_command(const bytes buffer) noexcept;

        /// error handler
        void on_error(const error_code error) noexcept;

        /// inform client about an error, type of error is determined from the error category
        void send_error(const error_code error) noexcept;

        /// inform client about internal error or out of memory
        void send_server_error(const char * message) noexcept;

        /// inform client that command is unknown
        void send_unknown_command_error() noexcept;

        /// send standard response to the client
        void send_response(const cache::Response res);

        /// add single item to the send buffer
        void push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::version_type cas_value, bool send_cas);

        ostream_type serialize() noexcept { return ostream_type(this->send_buffer()); }

        cache::Command parse_command_name(const bytes command);
        void handle_retrieval_command(cache::Command cmd, bytes args_buf);
        void handle_storage_command(cache::Command cmd, bytes args_buf);
        void handle_delete_command(cache::Command cmd, bytes args_buf);
        void handle_arithmetic_command(cache::Command cmd, bytes args_buf);
        void handle_touch_command(cache::Command cmd, bytes args_buf);
        void handle_service_command(cache::Command cmd, bytes args_buf);

        void validate_key(const bytes key);
        bool maybe_noreply(const bytes buffer);

    private:
        cache::Cache & cache;
        const cache::HashFunction calc_hash;
        bool m_killed;
    };


    template <class Sock>
    inline void text_protocol_handler<Sock>::receive_command() noexcept {
        super::async_receive_until(CRLF,
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
    inline void text_protocol_handler<Sock>::send_error(const error_code error) noexcept {
        try {
            auto error_message = bytes(error.message().c_str(), error.message().size());
            this->send_buffer().discard_written();
            if (error.category() == get_protocol_error_category()) {
                serialize() << CLIENT_ERROR << ' ' << error_message;
            } else {
                serialize() << SERVER_ERROR << ' ' << error_message;
            }
            flush();
        } catch(const std::exception &) {}
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_server_error(const char * message) noexcept {
        try {
            this->send_buffer().discard_written();
            serialize() << SERVER_ERROR << ' ' << bytes(message, std::strlen(message));
            flush();
        } catch(const std::exception &) {}
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_unknown_command_error() noexcept {
        try {
            this->send_buffer().discard_written();
            serialize() << ERROR;
            flush();
        } catch(const std::exception &) {}
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_response(const cache::Response res) {
        serialize() << AsciiResponse(res) << CRLF;
        flush();
    }


    template <class Sock>
    void text_protocol_handler<Sock>::push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::version_type cas_value, bool send_cas) {
        const auto value_length = static_cast<unsigned>(value.length());
        static const bytes VALUE = bytes::from_literal("VALUE ");
        serialize() << VALUE << key << ' ' << flags << ' ' << value_length;
        if (send_cas) {
            serialize() << ' ' << cas_value;
        }
        serialize() << CRLF << value << CRLF;
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
            cache::Command cmd = parse_command_name(command_name);
            switch (ClassOfCommand(cmd)) {
                case cache::STORAGE_COMMAND:
                    handle_storage_command(cmd, command_args);
                    return;
                case cache::DELETE_COMMAND:
                    handle_delete_command(cmd, command_args);
                    break;
                case cache::ARITHMETIC_COMMAND:
                    handle_arithmetic_command(cmd, command_args);
                    break;
                case cache::TOUCH_COMMAND:
                    handle_touch_command(cmd, command_args);
                    break;
                case cache::RETRIEVAL_COMMAND:
                    handle_retrieval_command(cmd, command_args);
                    break;
                case cache::SERVICE_COMMAND:
                    handle_service_command(cmd, command_args);
                case cache::QUIT_COMMAND:
                    super::close();
                    suicide();
                    return;
                case cache::UNDEFINED_COMMAND:
                default:
                    send_unknown_command_error();
                    suicide();
                    return;
            }
            receive_command();
            return;
        } catch(const system_error & exc) {
            send_error(exc.code());
        } catch(const std::exception & exc) {
            send_server_error(exc.what());
        } catch(...) {
            send_server_error("Unknown error");
        }
        // if we reach here ...
        suicide();
    }


    template <class Sock>
    inline cache::Command text_protocol_handler<Sock>::parse_command_name(const bytes command) {
        const auto is_3 = [=](const char literal[4]) -> bool { return *command.nth(1) == literal[1] && *command.nth(2) == literal[2]; };
        const auto is_4 = [=](const char literal[5]) -> bool { return is_3(literal) && *command.nth(3) == literal[3]; };
        const auto is_5 = [=](const char literal[6]) -> bool { return is_4(literal) && *command.nth(4) == literal[4]; };
        const auto is_6 = [=](const char literal[7]) -> bool { return is_5(literal) && *command.nth(5) == literal[5]; };
        const auto is_7 = [=](const char literal[8]) -> bool { return is_6(literal) && *command.nth(6) == literal[6]; };

        if (command) {
            const char first_char = command[0];
            switch (command.length()) {
            case 3:
                switch (first_char) {
                case 'a': return is_3("add") ? cache::ADD : cache::UNDEFINED;
                case 'c': return is_3("cas") ? cache::CAS : cache::UNDEFINED;
                case 'g': return is_3("get") ? cache::GET : cache::UNDEFINED;
                case 's': return is_3("set") ? cache::SET : cache::UNDEFINED;
                default : return cache::UNDEFINED;
                }
            case 4:
                switch (first_char) {
                case 'd': return is_4("decr") ? cache::DECR : cache::UNDEFINED;
                case 'g': return is_4("gets") ? cache::GETS : cache::UNDEFINED;
                case 'i': return is_4("incr") ? cache::INCR : cache::UNDEFINED;
                case 'q': return is_4("quit") ? cache::QUIT : cache::UNDEFINED;
                default : return cache::UNDEFINED;
                }
            case 5:
                return is_5("touch") ? cache::TOUCH : cache::UNDEFINED;
            case 6:
                switch (first_char) {
                case 'a': return is_6("append") ? cache::APPEND : cache::UNDEFINED;
                case 'd': return is_6("delete") ? cache::DELETE : cache::UNDEFINED;
                default : return cache::UNDEFINED;
                }
            case 7:
                switch (first_char) {
                case 'p': return is_7("prepend") ? cache::PREPEND : cache::UNDEFINED;
                case 'r': return is_7("replace") ? cache::REPLACE : cache::UNDEFINED;
                default : return cache::UNDEFINED;
                }
            default :
                return cache::UNDEFINED;
            }
        }
        return cache::UNDEFINED;
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_retrieval_command(cache::Command cmd, bytes args_buf) {
        const bool send_cas = cmd == cache::GETS ? true : false;
        do {
            bytes key;
            tie(key, args_buf) = args_buf.split(SPACE);
            validate_key(key);
            cache.do_get(key, calc_hash(key),
                         [=](error_code cache_error, bool found, bytes value, cache::opaque_flags_type flags, cache::version_type cas_value) {
                             try {
                                 if (not cache_error && found) {
                                     push_item(key, value, flags, cas_value, send_cas);
                                 } else if (cache_error) {
                                     send_error(cache_error);
                                     suicide();
                                 }
                             } catch (const std::exception & exc) {
                                 send_server_error(exc.what());
                                 suicide();
                             }
                         });
        } while (args_buf && not m_killed);
        if (not m_killed) {
            static const bytes END = bytes::from_literal("END");
            serialize() << END << CRLF;
            flush();
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_command(cache::Command cmd, bytes arguments_buf) {
        try {
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
                auto errc = make_protocol_error(error::value_length);
                throw system_error(errc);
            }
            cache::version_type cas_unique = 0;
            if (cmd == cache::CAS) {
                tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
                cas_unique = str_to_int<cache::version_type>(parsed.begin(), parsed.length());
            }
            bool noreply = maybe_noreply(arguments_buf);
            if (not arguments_buf.empty()) {
                throw system_error(make_protocol_error(error::crlf_expected));
            }
            // create new Item
            error_code alloc_error; cache::Item * new_item;
            auto expiration = (expires_after == cache::seconds(0)) ? cache::expiration_time_point::max() : cache::clock::now() + expires_after;
            tie(alloc_error, new_item) = cache.create_item(key, calc_hash(key), datalen, flags, expiration, cas_unique);
            if (alloc_error) {
                send_error(alloc_error);
                receive_command();
                return;
            }

            // read value
            super::async_receive_n(datalen + CRLF.length(),
                [=](const error_code net_error, const bytes data) noexcept {
                    if (net_error) {
                        cache.destroy_item(new_item);
                        on_error(net_error);
                        return;
                    }
                    if (not data.endswith(CRLF)) {
                        cache.destroy_item(new_item);
                        send_error(make_protocol_error(error::value_crlf_expected));
                        suicide();
                        return;
                    }
                    new_item->assign_value(data.rtrim_n(CRLF.length()));
                    error_code cache_error; cache::Response response;
                    tie(cache_error, response) = cache.do_storage(cmd, new_item);
                    if (not cache_error) {
                        if (not noreply) { send_response(response); }
                    } else {
                        cache.destroy_item(new_item);
                        send_error(cache_error);
                    }
                    receive_command();
                });

            return;

        } catch (const std::overflow_error & ) {
            send_error(make_protocol_error(error::integer_range));
        } catch (const std::invalid_argument & ) {
            send_error(make_protocol_error(error::integer_conv));
        }
        // if we reach here...
        suicide();
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_delete_command(cache::Command /*cmd*/, bytes args_buf) {
        bytes key = args_buf;
        validate_key(key);
        error_code cache_error; cache::Response response;
        tie(cache_error, response) = cache.do_delete(key, calc_hash(key));
        if (not cache_error) {
            send_response(response);
        } else {
            send_error(cache_error);
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_arithmetic_command(cache::Command cmd, bytes args_buf) {
        (void) cmd; (void) args_buf;
        // TODO: Implementation
        send_error(::cachelot::error::not_implemented);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_touch_command(cache::Command cmd, bytes args_buf) {
        (void) cmd; (void) args_buf;
        // TODO: Implementation
        send_error(::cachelot::error::not_implemented);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_service_command(cache::Command cmd, bytes args_buf) {
        (void) cmd; (void) args_buf;
        send_error(::cachelot::error::not_implemented);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::validate_key(const bytes key) {
        if (not key) {
            throw system_error(make_protocol_error(error::key_expected));
        }
        if (key.length() > cache::Item::max_key_length) {
            throw system_error(make_protocol_error(error::key_length));
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
            throw system_error(make_protocol_error(error::noreply_expected));
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::on_error(const error_code error) noexcept {
        debug_assert(error);
        (void) error;
        suicide();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::suicide() noexcept {
        // 'suicide' can be called several times from background operation handlers
        // we must ensure that it'll be only one attempt to delete this object
        if (! m_killed) {
            super::close();
            m_killed = true;
            super::post([=](){ delete this; });
        }
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::flush() noexcept {
        super::async_send_all([=](const error_code error) {
            if (error) {
                this->on_error(error);
            }
        });
    }

} } // namespace namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_TEXT

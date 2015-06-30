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
    constexpr bytes END = bytes::from_literal("END");
    constexpr char SPACE = ' ';

    /// Memcached error types
    constexpr bytes ERROR = bytes::from_literal("ERROR"); ///< unknown command
    constexpr bytes CLIENT_ERROR = bytes::from_literal("CLIENT_ERROR"); ///< request is ill-formed
    constexpr bytes SERVER_ERROR = bytes::from_literal("SERVER_ERROR"); ///< internal server error

    template <class SocketType>
    class text_protocol_handler : public stream_connection<SocketType, text_protocol_handler<SocketType>> {
        typedef stream_connection<SocketType, text_protocol_handler<SocketType>> super;
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
        typedef SocketType socket_type;

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

        /// inform client about an error
        /// @p err_type - Type of the error: client or server
        /// @p err_msg - Human readable error message
        void send_error(bytes err_type, bytes err_msg) noexcept;

        /// inform client about an error, type of error is determined from the error category
        void send_error(const error_code error) noexcept;

        /// inform client about internal error or out of memory
        void send_server_error(const char * message) noexcept;

        /// inform client that command is unknown
        void send_unknown_command_error() noexcept;

        /// send cache `response` to the client
        void send_response(const cache::Response response);

        /// reply to the client either with an `error` or with the `response` if `noreply` is not set
        void reply_to_client(const error_code error, const cache::Response response, bool noreply);

        /// add single item to the send buffer
        void push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::version_type cas_value, bool send_cas);

        /// return text serialization stream
        ostream_type serialize() noexcept { return ostream_type(super::send_buffer()); }

        cache::Command parse_command_name(const bytes command);
        void handle_retrieval_command(cache::Command cmd, bytes args_buf);
        void handle_storage_command(cache::Command cmd, bytes args_buf);
        void handle_delete_command(cache::Command cmd, bytes args_buf);
        void handle_arithmetic_command(cache::Command cmd, bytes args_buf);
        void handle_touch_command(cache::Command cmd, bytes args_buf);
        void handle_statistics_command(cache::Command cmd, bytes args_buf);

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
                if (not error) {
                    debug_assert(data.endswith(CRLF));
                    this->on_command(data);
                } else {
                    this->on_error(error);
                }
        });
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_error(bytes err_type, bytes err_msg) noexcept {
        try {
            super::send_buffer().discard_written();
            serialize() << err_type << ' ' << err_msg;
            flush();
        } catch(const std::exception &) {
            suicide();
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_error(const error_code error) noexcept {
        auto msg = error.message();
        if (error.category() == get_protocol_error_category()) {
            send_error(CLIENT_ERROR, bytes(msg.c_str(), msg.size()));
        } else {
            send_error(SERVER_ERROR, bytes(msg.c_str(), msg.size()));
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_server_error(const char * message) noexcept {
        send_error(SERVER_ERROR, bytes(message, std::strlen(message)));
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_unknown_command_error() noexcept {
        try {
            serialize() << ERROR;
            flush();
        } catch(const std::exception &) {}
    }


    template <class Sock>
    void text_protocol_handler<Sock>::send_response(const cache::Response response) {
        serialize() << AsciiResponse(response) << CRLF;
        flush();
    }


    template <class Sock>
    void text_protocol_handler<Sock>::reply_to_client(const error_code error, const cache::Response response, bool noreply) {
        if (not error) {
            if (not noreply) { send_response(response); }
        } else {
            send_error(error);
        }
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
            switch (cmd) {
                case cache::ADD:
                case cache::APPEND:
                case cache::CAS:
                case cache::PREPEND:
                case cache::REPLACE:
                case cache::SET:
                    handle_storage_command(cmd, command_args);
                    return;
                case cache::DELETE:
                    handle_delete_command(cmd, command_args);
                    break;
                case cache::INCR:
                case cache::DECR:
                    handle_arithmetic_command(cmd, command_args);
                    break;
                case cache::TOUCH:
                    handle_touch_command(cmd, command_args);
                    break;
                case cache::GET:
                case cache::GETS:
                    handle_retrieval_command(cmd, command_args);
                    break;
                case cache::STATS:
                    handle_statistics_command(cmd, command_args);
                    break;
                case cache::QUIT:
                    suicide();
                    return;
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
                switch (first_char) {
                case 't': return is_5("touch") ? cache::TOUCH : cache::UNDEFINED;
                case 's': return is_5("stats") ? cache::STATS : cache::UNDEFINED;
                default : return cache::UNDEFINED;
                }
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
                         [=](error_code cache_error, bool found, bytes value, cache::opaque_flags_type flags, cache::version_type cas_value) noexcept {
                             try {
                                 if (not cache_error && found) {
                                     push_item(key, value, flags, cas_value, send_cas);
                                     return;
                                 } else if (cache_error) {
                                     throw system_error(cache_error);
                                 }
                             } catch (const std::exception & exc) {
                                 super::send_buffer().discard_written();
                                 send_server_error(exc.what());
                                 suicide();
                             }
                         });
        } while (args_buf && not m_killed);
        if (not m_killed) {
            serialize() << END << CRLF;
            flush();
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_command(cache::Command cmd, bytes arguments_buf) {
        // command arguments
        bytes key;
        tie(key, arguments_buf) = arguments_buf.split(SPACE);
        validate_key(key);
        bytes parsed;
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        cache::opaque_flags_type flags = str_to_int<cache::opaque_flags_type>(parsed.begin(), parsed.end());
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        auto expires_after = cache::seconds(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
        tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
        uint32 datalen = str_to_int<uint32>(parsed.begin(), parsed.end());
        if (datalen > settings.cache.max_value_size) {
            throw system_error(error::value_length);
        }
        cache::version_type cas_unique = 0;
        if (cmd == cache::CAS) {
            tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
            cas_unique = str_to_int<cache::version_type>(parsed.begin(), parsed.end());
        }
        bool noreply = maybe_noreply(arguments_buf);
        if (not arguments_buf.empty()) {
            throw system_error(error::crlf_expected);
        }
        // create new Item
        error_code alloc_error; cache::Item * new_item;
        auto expiration = (expires_after == cache::seconds(0)) ? cache::expiration_time_point::max() : cache::clock::now() + expires_after;
        tie(alloc_error, new_item) = cache.create_item(key, calc_hash(key), datalen, flags, expiration, cas_unique);
        if (alloc_error) {
            // TODO: discard receive buffer!!!
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
                    send_error(error::value_crlf_expected);
                    suicide();
                    return;
                }
                new_item->assign_value(data.rtrim_n(CRLF.length()));
                error_code cache_error; cache::Response response;
                tie(cache_error, response) = cache.do_storage(cmd, new_item);
                if (cache_error) {
                    cache.destroy_item(new_item);
                }
                reply_to_client(cache_error, response, noreply);
                receive_command();
            });
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_delete_command(cache::Command cmd, bytes args_buf) {
        debug_assert(cmd == cache::DELETE); (void)cmd;
        // parse
        bytes key;
        tie(key, args_buf) = args_buf.split(SPACE);
        validate_key(key);
        bool noreply = maybe_noreply(args_buf);
        // access cache
        error_code cache_error; cache::Response response;
        tie(cache_error, response) = cache.do_delete(key, calc_hash(key));
        reply_to_client(cache_error, response, noreply);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_arithmetic_command(cache::Command cmd, bytes args_buf) {
        debug_assert(cmd == cache::INCR || cmd == cache::DECR);
        bytes key;
        tie(key, args_buf) = args_buf.split(SPACE);
        validate_key(key);
        bytes parsed;
        tie(parsed, args_buf) = args_buf.split(SPACE);
        auto delta = str_to_int<uint64>(parsed.begin(), parsed.end());
        bool noreply = maybe_noreply(args_buf);
        // access cache
        error_code error; cache::Response response; uint64 new_value;
        tie(error, response, new_value) = cache.do_arithmetic(cmd, key, calc_hash(key), delta);
        if (noreply) {
            return;
        }
        if (not error) {
            if  (response == cache::STORED) {
                serialize() << new_value << CRLF;
                flush();
            } else {
                debug_assert(response == cache::NOT_FOUND);
                send_response(response);
            }
        } else {
            send_error(error);
            return;
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_touch_command(cache::Command cmd, bytes args_buf) {
        debug_assert(cmd == cache::TOUCH); (void)cmd;
        // parse
        bytes key;
        tie(key, args_buf) = args_buf.split(SPACE);
        validate_key(key);
        bytes parsed;
        tie(parsed, args_buf) = args_buf.split(SPACE);
        cache::seconds keep_alive_duration(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
        bool noreply = maybe_noreply(args_buf);
        // access cache
        error_code error; cache::Response response;
        tie(error, response) = cache.do_touch(key, calc_hash(key), keep_alive_duration);
        reply_to_client(error, response, noreply);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_statistics_command(cache::Command cmd, bytes args_buf) {
        debug_assert(cmd == cache::STATS); (void)cmd;
        static const bytes STAT = bytes::from_literal("STAT ");
        if (args_buf.empty()) {
            cache.publish_stats();
            #define SERIALIZE_STAT(stat_group, stat_type, stat_name, stat_description) \
            serialize() << STAT << bytes::from_literal(CACHELOT_PP_STR(stat_name)) << ' ' << STAT_GET(stat_group, stat_name) << CRLF;

            #define SERIALIZE_CACHE_STAT(typ, name, desc) SERIALIZE_STAT(cache, typ, name, desc)
            CACHE_STATS(SERIALIZE_CACHE_STAT)
            #undef SERIALIZE_CACHE_STAT

            #define SERIALIZE_MEM_STAT(typ, name, desc) SERIALIZE_STAT(mem, typ, name, desc)
            MEMORY_STATS(SERIALIZE_MEM_STAT)
            #undef SERIALIZE_MEM_STAT

            #undef SERIALIZE_STAT
            serialize() << END << CRLF;
            flush();
        } else {
            send_error(::cachelot::error::not_implemented);
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::validate_key(const bytes key) {
        if (not key) {
            throw system_error(error::key_expected);
        }
        if (key.length() > cache::Item::max_key_length) {
            throw system_error(error::key_length);
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
            throw system_error(error::noreply_expected);
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
        // it must be re-entrable
        if (not m_killed) {
            m_killed = true;
            super::close();
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

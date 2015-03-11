#ifndef CACHELOT_PROTO_MEMCACHED_TEXT
#define CACHELOT_PROTO_MEMCACHED_TEXT

#include <cachelot/proto_memcached.h>

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

        /// send erroneous response to the client
        void send_error(const ErrorType errtype, const bytes error_msg) noexcept;

        /// send error message of code `error` as server error to the client
        void send_server_error(const error_code error) noexcept;

        /// send standard response to the client
        void send_response(const cache::Response res) noexcept;

        /// add single item to the send buffer
        void push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::cas_value_type cas_value, bool send_cas) noexcept;

        ostream_type serialize() noexcept { return ostream_type(this->send_buffer()); }

        cache::Command parse_command_name(const bytes command);
        void handle_storage_command_header(cache::Command cmd, bytes args_buf);
        void handle_storage_command_data(cache::Command cmd, bytes key, bytes value, cache::opaque_flags_type flags, cache::seconds expires_after, cache::cas_value_type cas_unique, bool noreply);
        void handle_delete_command(cache::Command cmd, bytes args_buf);
        void handle_arithmetic_command(cache::Command cmd, bytes args_buf);
        void handle_touch_command(cache::Command cmd, bytes args_buf);
        void handle_retrieval_command(cache::Command cmd, bytes args_buf);
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
    inline void text_protocol_handler<Sock>::send_error(const ErrorType etype, const bytes error_msg) noexcept {
        serialize() << AsciiErrorType(etype) << ' ' << error_msg << CRLF;
        flush();
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_server_error(const error_code error) noexcept {
        auto error_message = bytes(error.message().c_str(), error.message().size());
        send_error(SERVER_ERROR, error_message);
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::send_response(const cache::Response res) noexcept {
        serialize() << AsciiResponse(res) << CRLF;
        flush();
    }


    template <class Sock>
    void text_protocol_handler<Sock>::push_item(const bytes key, bytes value, cache::opaque_flags_type flags, cache::cas_value_type cas_value, bool send_cas) noexcept {
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
                    handle_storage_command_header(cmd, command_args);
                    return;
                case cache::DELETE_COMMAND:
                    break;
                case cache::ARITHMETIC_COMMAND:
                    //handle_arithmetic_command(command_args);
                    break;
                case cache::TOUCH_COMMAND:
                    //handle_touch_command(command_args);
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
                default:
                    throw non_existing_command_error();
            }
            receive_command();
            return;
        } catch(const cache_error & err) {
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
    inline cache::Command text_protocol_handler<Sock>::parse_command_name(const bytes command) {
        if (not command) {
            return cache::UNDEFINED;
        }
        const auto is_ = [=](const char * literal) -> bool {
            return std::strncmp(command.begin(), literal, command.length()) == 0;
        };
        const char first_char = command[0];
        switch (command.length()) {
        case 3:
            switch (first_char) {
            case 'a': return is_("add") ? cache::ADD : cache::UNDEFINED;
            case 'c': return is_("cas") ? cache::CAS : cache::UNDEFINED;
            case 'g': return is_("get") ? cache::GET : cache::UNDEFINED;
            case 's': return is_("set") ? cache::SET : cache::UNDEFINED;
            default : return cache::UNDEFINED;
            }
        case 4:
            switch (first_char) {
            case 'd': return is_("decr") ? cache::DECR : cache::UNDEFINED;
            case 'g': return is_("gets") ? cache::GETS : cache::UNDEFINED;
            case 'i': return is_("incr") ? cache::INCR : cache::UNDEFINED;
            case 'q': return is_("quit") ? cache::QUIT : cache::UNDEFINED;
            default : return cache::UNDEFINED;
            }
        case 5:
            return is_("touch") ? cache::TOUCH : cache::UNDEFINED;
        case 6:
            switch (first_char) {
            case 'a': return is_("append") ? cache::APPEND : cache::UNDEFINED;
            case 'd': return is_("delete") ? cache::DELETE : cache::UNDEFINED;
            default : return cache::UNDEFINED;
            }
        case 7:
            switch (first_char) {
            case 'p': return is_("prepend") ? cache::PREPEND : cache::UNDEFINED;
            case 'r': return is_("replace") ? cache::REPLACE : cache::UNDEFINED;
            default : return cache::UNDEFINED;
            }
        default : 
            return cache::UNDEFINED;
        }
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_command_header(cache::Command cmd, bytes arguments_buf) {
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
        if (cmd == cache::CAS) {
            tie(parsed, arguments_buf) = arguments_buf.split(SPACE);
            cas_unique = str_to_int<cache::cas_value_type>(parsed.begin(), parsed.length());
        }
        bool noreply = maybe_noreply(arguments_buf);
        if (not arguments_buf.empty()) {
            throw client_error("invalid command: \\r\\n expected");
        }
        // read value
        super::async_receive_n(datalen + CRLF.length(),
            [=](const error_code net_error, const bytes data) noexcept {
                if (not net_error) {
                    bytes value;
                    if (data.endswith(CRLF)) {
                        value = data.rtrim_n(CRLF.length());
                    } else {
                        send_error(CLIENT_ERROR, bytes::from_literal("invalid value: \\r\\n expected"));
                        suicide();
                    }
                    handle_storage_command_data(cmd, key, value, flags, expires_after, cas_unique, noreply);
                } else {
                    on_error(net_error);
                }
            });
    }


    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_storage_command_data(cache::Command cmd, bytes key, bytes value, cache::opaque_flags_type flags, cache::seconds expires_after, cache::cas_value_type cas_unique, bool noreply) {
        const auto hash = calc_hash(key);
        error_code cache_error; cache::Response response;
        tie(cache_error, response) = cache.do_storage(cmd, key, hash, value, flags, expires_after, cas_unique);
        if (not cache_error) {
            if (not noreply) {
                send_response(response);
            }
        } else {
            send_server_error(cache_error);
        }
        receive_command();
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_retrieval_command(cache::Command cmd, bytes args_buf) {
        const bool send_cas = cmd == cache::GETS ? true : false;
        do {
            bytes key;
            tie(key, args_buf) = args_buf.split(SPACE);
            validate_key(key);
            cache.do_get(key, calc_hash(key),
                [=](error_code cache_error, bool found, bytes value, cache::opaque_flags_type flags, cache::cas_value_type cas_value) {
                    // TODO: What if error?
                    if (not cache_error && found) { push_item(key, value, flags, cas_value, send_cas); }
                });
        } while (args_buf);
        static const bytes END = bytes::from_literal("END");
        serialize() << END << CRLF;
        this->flush();
    }

    template <class Sock>
    inline void text_protocol_handler<Sock>::handle_service_command(cache::Command cmd, bytes args_buf) {
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
        if (! m_killed) {
            m_killed = true;
            // TODO: Race! Unprocessed AsyncRequests in Cache would get invalid `this` pointer in their callbacks
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

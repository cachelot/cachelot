#include <cachelot/common.h>
#include <server/memcached/proto_ascii.h>


namespace cachelot {

    namespace memcached { namespace ascii {

        constexpr char SPACE = ' ';
        constexpr slice CRLF = slice::from_literal("\r\n");
        constexpr slice VALUE = slice::from_literal("VALUE");
        constexpr slice END = slice::from_literal("END");
        constexpr slice NOREPLY = slice::from_literal("noreply");
        constexpr slice STAT = slice::from_literal("STAT");
        constexpr slice VERSION =  slice::from_literal("VERSION");
        constexpr slice OK =  slice::from_literal("OK");

        /// Memcached error types
        constexpr slice ERROR = slice::from_literal("ERROR"); ///< unknown command
        constexpr slice CLIENT_ERROR = slice::from_literal("CLIENT_ERROR"); ///< request is ill-formed
        constexpr slice SERVER_ERROR = slice::from_literal("SERVER_ERROR"); ///< internal server error

        /// Handle on of the `get` `gets` commands
        net::ConversationReply handle_retrieval_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle on of the: `add`, `set`, `replace`, `cas`, `append`, `prepend` commands
        net::ConversationReply handle_storage_command(Command cmd, slice args, io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle the `delete` command
        net::ConversationReply handle_delete_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle on of the: `incr` `decr` commands
        net::ConversationReply handle_arithmetic_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle the `touch` command
        net::ConversationReply handle_touch_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle the `stats` command
        net::ConversationReply handle_statistics_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle the `version` command
        net::ConversationReply handle_version_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Handle the `flush` command
        net::ConversationReply handle_flush_all_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api);

        /// Write one of the cache responses if `noreply` is not specified, none otherwise
        net::ConversationReply reply_with_response(io_buffer & send_buf, Response response, bool noreply);

        /// Parse the name of the `command`
        Command parse_command_name(slice command) noexcept;

        /// Read key value from the arguments sequence
        /// @return the key and rest of the args
        tuple<slice, slice> parse_key(slice args);

        /// parse optional `noreply` clause
        bool maybe_noreply(const slice buffer);

        // hash function
        inline cache::hash_type calc_hash(const slice key) noexcept {
            cache::HashFunction do_calc_hash;
            return do_calc_hash(key);
        }


        // Stream operator to serialize `slice`
        inline io_buffer & operator<<(io_buffer & buf, const slice value) {
            auto dest = buf.begin_write(value.length());
            std::memcpy(dest, value.begin(), value.length());
            buf.confirm_write(value.length());
            return buf;
        }


        // Stream operator to serialize `string`
        inline io_buffer & operator<<(io_buffer & buf, const string value) { return buf << slice(&value.front(), value.length()); }

        // Stream operator to serialize `c string`
        inline io_buffer & operator<<(io_buffer & buf, const char * value) { return buf << slice(value, std::strlen(value)); }


        // Stream operator to serialize single `char`
        inline io_buffer & operator<<(io_buffer & buf, const char value) {
            auto dest = buf.begin_write(sizeof(char));
            *dest = value;
            buf.confirm_write(sizeof(char));
            return buf;
        }


        // Stream operator to serialize single `bool`
        inline io_buffer & operator<<(io_buffer & buf, const bool value) {
            if (value) return buf << '1'; else return buf << '0';
        }


        // Stream operator to serialize cache response as an ascii string
        inline io_buffer & operator<<(io_buffer & buf, Response resp) {
            return buf << AsciiResponse(resp);
        }


        /// Heler function to write integer into io_buffer as an ascii string
        template<typename IntType>
        inline io_buffer & serialize_integer(io_buffer & buf, const IntType x) {
            auto dest = buf.begin_write(internal::numeric<IntType>::max_str_length);
            size_t written = int_to_str(x, dest);
            buf.confirm_write(written);
            return buf;
        }
        #define __DO_SERIALIZE_INTEGER_ASCII(IntType)                           \
                                                                                \
        inline io_buffer & operator<<(io_buffer & buf, const IntType value) {   \
            return serialize_integer<IntType>(buf, value);                      \
        }

        __DO_SERIALIZE_INTEGER_ASCII(uint16)
        __DO_SERIALIZE_INTEGER_ASCII(uint32)
        __DO_SERIALIZE_INTEGER_ASCII(uint64)
        #undef __DO_SERIALIZE_INTEGER_ASCII


        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api) noexcept {
            auto r_savepoint = recv_buf.read_savepoint();
            auto w_savepoint = send_buf.write_savepoint();
            try {
                // read command header <cmd> <key> <args...>\r\n
                slice header = recv_buf.try_read_until(CRLF);
                if (header.empty()) {
                    throw system_error(error::incomplete_request);
                }
                debug_assert(header.endswith(CRLF));
                header = header.rtrim_n(CRLF.length());

                // determine command name
                slice ascii_cmd, args;
                tie(ascii_cmd, args) = header.split(SPACE);
                auto command = parse_command_name(ascii_cmd);
                net::ConversationReply reply;
                // process the command
                switch (command) {
                // retrieval command
                case Command::GET:
                case Command::GETS:
                    reply = handle_retrieval_command(command, args, send_buf, cache_api);
                    break;
                // storage command
                case Command::ADD:
                case Command::APPEND:
                case Command::CAS:
                case Command::PREPEND:
                case Command::REPLACE:
                case Command::SET:
                    reply = handle_storage_command(command, args, recv_buf, send_buf, cache_api);
                    break;
                // delete
                case Command::DELETE:
                    reply = handle_delete_command(command, args, send_buf, cache_api);
                    break;
                // arithmetic
                case Command::INCR:
                case Command::DECR:
                    reply = handle_arithmetic_command(command, args, send_buf, cache_api);
                    break;
                // touch
                case Command::TOUCH:
                    reply = handle_touch_command(command, args, send_buf, cache_api);
                    break;
                // statistics retrieval
                case Command::STATS:
                    reply = handle_statistics_command(command, args, send_buf, cache_api);
                    break;
                case Command::VERSION:
                    reply = handle_version_command(command, args, send_buf, cache_api);
                    break;
                case Command::FLUSH_ALL:
                    reply = handle_flush_all_command(command, args, send_buf, cache_api);
                    break;
                // terminate session
                case Command::QUIT:
                    return net::CLOSE_IMMEDIATELY;
                // unknown command
                default:
                    throw system_error(error::broken_request);
                }
                return reply;

            } catch (const system_error & syserr) {
                // discard any written data to write error message instead
                send_buf.rollback_write_transaction(w_savepoint);
                const auto errmsg = syserr.code().message();
                if (syserr.code().category() == get_protocol_error_category()) {
                    // protocol error
                    send_buf << CLIENT_ERROR << SPACE << errmsg << CRLF;
                    // ill-formed packet, swallow recv_buf data
                    recv_buf.reset();
                    return net::SEND_REPLY_AND_READ;
                } else {
                    // server error
                    switch (syserr.code().value()) {
                    case error::incomplete_request:
                        // rollback read position, start over when more data will come
                        recv_buf.rollback_read_transaction(r_savepoint);
                        return net::READ_MORE;
                    case error::broken_request:
                        // ill-formed packet, swallow recv_buf data
                        recv_buf.read_all();
                        send_buf << ERROR << CRLF;
                        return net::SEND_REPLY_AND_READ;
                    case error::numeric_convert:
                    case error::numeric_overflow:
                        // numeric errors are considered as a client fault
                        send_buf << CLIENT_ERROR << SPACE << errmsg << CRLF;
                        return net::SEND_REPLY_AND_READ;
                    default:
                        // internal server error
                        send_buf << SERVER_ERROR << SPACE << errmsg << CRLF;
                        return net::SEND_REPLY_AND_READ;
                    }
                }
            } catch (const std::exception & exc) {
                // discard any written data to write error message instead
                send_buf.rollback_write_transaction(w_savepoint);
                send_buf << SERVER_ERROR << SPACE << exc.what() << CRLF;
                return net::SEND_REPLY_AND_READ;
            }
        }


        inline tuple<slice, slice> parse_key(slice args) {
            slice key;
            tie(key, args) = args.split(SPACE);
            validate_key(key);
            return make_tuple(key, args);
        }


        inline bool maybe_noreply(const slice args) {
            if (args.empty()) {
                return false;
            } else if (args == NOREPLY) {
                return true;
            } else {
                throw system_error(error::noreply_expected);
            }
        }


        inline net::ConversationReply handle_retrieval_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            do {
                slice key; tie(key, args) = parse_key(args);
                auto i = cache_api.do_get(key, calc_hash(key));
                if (i) {
                    send_buf << VALUE << SPACE << i->key() << SPACE << i->opaque_flags() << SPACE << static_cast<uint32>(i->value().length());
                    if (cmd == Command::GETS) {
                        send_buf << SPACE << i->timestamp();
                    }
                    send_buf << CRLF << i->value() << CRLF;
                }
            } while (not args.empty());
            send_buf << END << CRLF;
            return net::SEND_REPLY_AND_READ;
        }


        inline net::ConversationReply handle_storage_command(Command cmd, slice args, io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api) {
            slice key; tie(key, args) = parse_key(args);
            slice parsed;
            tie(parsed, args) = args.split(SPACE);
            cache::opaque_flags_type flags = str_to_int<cache::opaque_flags_type>(parsed.begin(), parsed.end());
            tie(parsed, args) = args.split(SPACE);
            auto keep_alive_duration = cache::seconds(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
            tie(parsed, args) = args.split(SPACE);
            uint32 datalen = str_to_int<uint32>(parsed.begin(), parsed.end());
            if (datalen > settings.cache.page_size) {
                throw system_error(error::value_length);
            }
            cache::timestamp_type cas_unique = 0;
            if (cmd == Command::CAS) {
                tie(parsed, args) = args.split(SPACE);
                cas_unique = str_to_int<cache::timestamp_type>(parsed.begin(), parsed.end());
            }
            bool noreply = maybe_noreply(args);
            // read <value>\r\n
            if (recv_buf.non_read() < datalen + CRLF.length()) {
                // help buffer to grow up to the necessary size
                recv_buf.ensure_capacity(datalen + CRLF.length() - recv_buf.non_read());
                throw system_error(error::incomplete_request);
            }
            auto value = slice(recv_buf.begin_read(), datalen + CRLF.length());
            if (value.endswith(CRLF)) {
                value = value.rtrim_n(CRLF.length()); // strip trailing \r\n
                recv_buf.confirm_read(datalen + CRLF.length());
            } else {
                throw system_error(error::value_crlf_expected);
            }
            // create new item and execute the cache API
            auto new_item = cache_api.create_item(key, calc_hash(key), value.length(), flags, keep_alive_duration);
            new_item->assign_value(value);
            try {
                auto response = Response::NOT_A_RESPONSE;
                bool found = false; bool stored = false;
                switch (cmd) {
                case Command::SET:
                    cache_api.do_set(new_item);
                    response = Response::STORED;
                    break;
                case Command::ADD:
                    found = cache_api.do_add(new_item);
                    response = found ? Response::STORED : Response::NOT_STORED;
                    break;
                case Command::REPLACE:
                    found = cache_api.do_replace(new_item);
                    response = found ? Response::STORED : Response::NOT_STORED;
                    break;
                case Command::CAS:
                    tie(found, stored) = cache_api.do_cas(new_item, cas_unique);
                    if (found) {
                        response = stored ? Response::STORED : Response::EXISTS;
                    } else {
                        response = Response::NOT_FOUND;
                    }
                    break;
                case Command::APPEND:
                    found = cache_api.do_append(new_item);
                    response = found ? Response::STORED : Response::NOT_STORED;
                    break;
                case Command::PREPEND:
                    found = cache_api.do_prepend(new_item);
                    response = found ? Response::STORED : Response::NOT_STORED;
                    break;
                default:
                    debug_assert(false);
                    throw system_error(error::unknown_error);
                }
                if (response != Response::STORED) {
                    // Item was not stored in the cache
                    cache_api.destroy_item(new_item);
                }
                return reply_with_response(send_buf, response, noreply);
            } catch (...) {
                cache_api.destroy_item(new_item);
                throw;
            }
        }


        inline net::ConversationReply handle_delete_command(Command, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            slice key; tie(key, args) = parse_key(args);
            bool noreply = maybe_noreply(args);
            bool found = cache_api.do_delete(key, calc_hash(key));
            auto response = found ? Response::DELETED : Response::NOT_FOUND;
            return reply_with_response(send_buf, response, noreply);
        }


        inline net::ConversationReply handle_arithmetic_command(Command cmd, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            slice key; tie(key, args) = parse_key(args);
            slice parsed;
            tie(parsed, args) = args.split(SPACE);
            auto delta = str_to_int<uint64>(parsed.begin(), parsed.end());
            bool noreply = maybe_noreply(args);
            bool found; uint64 new_value;
            if (cmd == Command::INCR) {
                tie(found, new_value) = cache_api.do_incr(key, calc_hash(key), delta);
            } else {
                tie(found, new_value) = cache_api.do_decr(key, calc_hash(key), delta);
            }
            if (noreply) {
                return net::READ_MORE;
            }
            if (found) {
                send_buf << new_value << CRLF;
            } else {
                send_buf << Response::NOT_FOUND << CRLF;
            }
            return net::SEND_REPLY_AND_READ;
        }


        inline net::ConversationReply handle_touch_command(Command, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            slice key; tie(key, args) = parse_key(args);
            slice parsed;
            tie(parsed, args) = args.split(SPACE);
            cache::seconds keep_alive_duration(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
            bool noreply = maybe_noreply(args);
            bool found = cache_api.do_touch(key, calc_hash(key), keep_alive_duration);
            auto response = found ? Response::TOUCHED : Response::NOT_FOUND;
            return reply_with_response(send_buf, response, noreply);
        }


        inline net::ConversationReply handle_statistics_command(Command, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            if (not args.empty()) {
                throw system_error(error::not_implemented);
            }
            cache_api.publish_stats();
            #define SERIALIZE_STAT(stat_group, stat_type, stat_name, stat_description) \
                send_buf << STAT << SPACE << slice::from_literal(CACHELOT_PP_STR(stat_name)) << SPACE << STAT_GET(stat_group, stat_name) << CRLF;

            #define SERIALIZE_CACHE_STAT(typ, name, desc) SERIALIZE_STAT(cache, typ, name, desc)
            CACHE_STATS(SERIALIZE_CACHE_STAT)
            #undef SERIALIZE_CACHE_STAT

            #define SERIALIZE_MEM_STAT(typ, name, desc) SERIALIZE_STAT(mem, typ, name, desc)
            MEMORY_STATS(SERIALIZE_MEM_STAT)
            #undef SERIALIZE_MEM_STAT

            #undef SERIALIZE_STAT
            send_buf << END << CRLF;
            return net::SEND_REPLY_AND_READ;
        }


        inline net::ConversationReply handle_version_command(Command, slice args, io_buffer & send_buf, cache::Cache &) {
            if (not args.empty()) {
                throw system_error(error::crlf_expected);
            }
            send_buf << VERSION << SPACE << CACHELOT_VERSION_FULL << CRLF;
            return net::SEND_REPLY_AND_READ;
        }


        inline net::ConversationReply handle_flush_all_command(Command, slice args, io_buffer & send_buf, cache::Cache & cache_api) {
            bool noreply = maybe_noreply(args);
            cache_api.do_flush_all();
            if (noreply) {
                return net::READ_MORE;
            }
            send_buf << OK << CRLF;
            return net::SEND_REPLY_AND_READ;
        }


        inline net::ConversationReply reply_with_response(io_buffer & send_buf, Response response, bool noreply) {
            if (not noreply) {
                send_buf << response << CRLF;
                return net::SEND_REPLY_AND_READ;
            } else {
                return net::READ_MORE;
            }
        }


        inline Command parse_command_name(slice command) noexcept {
            static const auto is_3 = [=](const char literal[4], slice cmd) -> bool {  return cmd[1] == literal[1] && cmd[2] == literal[2]; };
            static const auto is_4 = [=](const char literal[5], slice cmd) -> bool {  return is_3(literal, cmd) && cmd[3] == literal[3]; };
            static const auto is_5 = [=](const char literal[6], slice cmd) -> bool {  return is_4(literal, cmd) && cmd[4] == literal[4]; };
            static const auto is_6 = [=](const char literal[7], slice cmd) -> bool {  return is_5(literal, cmd) && cmd[5] == literal[5]; };
            static const auto is_7 = [=](const char literal[8], slice cmd) -> bool {  return is_6(literal, cmd) && cmd[6] == literal[6]; };

            if (command) {
                const char first_char = command[0];
                switch (command.length()) {
                case 3:
                    switch (first_char) {
                    case 'a': return is_3("add", command) ? Command::ADD : Command::UNDEFINED;
                    case 'c': return is_3("cas", command) ? Command::CAS : Command::UNDEFINED;
                    case 'g': return is_3("get", command) ? Command::GET : Command::UNDEFINED;
                    case 's': return is_3("set", command) ? Command::SET : Command::UNDEFINED;
                    default : return Command::UNDEFINED;
                    }
                case 4:
                    switch (first_char) {
                    case 'd': return is_4("decr", command) ? Command::DECR : Command::UNDEFINED;
                    case 'g': return is_4("gets", command) ? Command::GETS : Command::UNDEFINED;
                    case 'i': return is_4("incr", command) ? Command::INCR : Command::UNDEFINED;
                    case 'q': return is_4("quit", command) ? Command::QUIT : Command::UNDEFINED;
                    default : return Command::UNDEFINED;
                    }
                case 5:
                    switch (first_char) {
                    case 't': return is_5("touch", command) ? Command::TOUCH : Command::UNDEFINED;
                    case 's': return is_5("stats", command) ? Command::STATS : Command::UNDEFINED;
                    default : return Command::UNDEFINED;
                    }
                case 6:
                    switch (first_char) {
                    case 'a': return is_6("append", command) ? Command::APPEND : Command::UNDEFINED;
                    case 'd': return is_6("delete", command) ? Command::DELETE : Command::UNDEFINED;
                    default : return Command::UNDEFINED;
                    }
                case 7:
                    switch (first_char) {
                    case 'p': return is_7("prepend", command) ? Command::PREPEND : Command::UNDEFINED;
                    case 'r': return is_7("replace", command) ? Command::REPLACE : Command::UNDEFINED;
                    case 'v': return is_7("version", command) ? Command::VERSION : Command::UNDEFINED;
                    default : return Command::UNDEFINED;
                    }
                case 9:
                    return std::strncmp("flush_all", command.begin(), 9) == 0 ? Command::FLUSH_ALL : Command::UNDEFINED;
                default :
                    return Command::UNDEFINED;
                }
            }
            return Command::UNDEFINED;
        }


    }} // namespace memcached::ascii

} // namespace cachelot


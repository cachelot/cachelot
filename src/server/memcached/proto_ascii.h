#ifndef CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED
#define CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <server/memcached/memcached.h>

/// @ingroup memcached 
/// @{

namespace cachelot {

    namespace memcached { namespace ascii {


        class protocol_parser : public basic_protocol_parser {
        public:
            /// @copydoc protocol_parser::parse_command_name
            virtual cache::Command parse_command_name(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_retrieval_command()
            virtual bytes parse_retrieval_command(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_retrieval_command_key()
            virtual tuple<bytes, bytes> parse_retrieval_command_keys(bytes keys) override;

            /// @copydoc protocol_parser::parse_storage_command()
            virtual tuple<bytes, bytes, cache::opaque_flags_type, cache::expiration_time_point, cache::version_type, bool> parse_storage_command(cache::Command cmd, io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_delete_command()
            virtual tuple<bytes, bool> parse_delete_command(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_arithmetic_command()
            virtual tuple<bytes, uint64, bool> parse_arithmetic_command(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_touch_command()
            virtual tuple<bytes, cache::expiration_time_point, bool> parse_touch_command(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::parse_statistics_command()
            virtual bytes parse_statistics_command(io_buffer & recv_buf) override;

            /// @copydoc protocol_parser::write_item()
            virtual void write_item(io_buffer & send_buf, cache::ItemPtr item, bool with_cas) override;

            /// @copydoc protocol_parser::finalize_batch()
            virtual void finalize_batch(io_buffer & send_buf) override;

            /// @copydoc protocol_parser::write_response()
            virtual void write_response(io_buffer & send_buf, cache::Response response) override;

            /// @copydoc protocol_parser::write_unknown_command_error()
            virtual void write_unknown_command_error(io_buffer & send_buf) override;

            /// @copydoc protocol_parser::write_client_error()
            virtual void write_client_error(io_buffer & send_buf, bytes message) override;
            
            /// @copydoc protocol_parser::write_server_error()
            virtual void write_server_error(io_buffer & send_buf, bytes message) override;

        private:
            /// read data from the buffer until CRLF found
            bytes parse_header(io_buffer & recv_buf);

            /// parse optional `noreply` clause
            bool maybe_noreply(const bytes buffer);
        };


        constexpr char SPACE = ' ';
        constexpr bytes CRLF = bytes::from_literal("\r\n");
        constexpr bytes VALUE = bytes::from_literal("VALUE");
        constexpr bytes END = bytes::from_literal("END");
        constexpr bytes NOREPLY = bytes::from_literal("noreply");

        /// Memcached error types
        constexpr bytes ERROR = bytes::from_literal("ERROR"); ///< unknown command
        constexpr bytes CLIENT_ERROR = bytes::from_literal("CLIENT_ERROR"); ///< request is ill-formed
        constexpr bytes SERVER_ERROR = bytes::from_literal("SERVER_ERROR"); ///< internal server error


        /// Parse the name of the `command`
        inline cache::Command protocol_parser::parse_command_name(io_buffer & recv_buf) {
            bytes command, _;
            tie(command, _) = bytes(recv_buf.begin_read(), recv_buf.non_read()).split(SPACE);
            static const auto is_3 = [=](const char literal[4]) -> bool { return *command.nth(1) == literal[1] && *command.nth(2) == literal[2]; };
            static const auto is_4 = [=](const char literal[5]) -> bool { return is_3(literal) && *command.nth(3) == literal[3]; };
            static const auto is_5 = [=](const char literal[6]) -> bool { return is_4(literal) && *command.nth(4) == literal[4]; };
            static const auto is_6 = [=](const char literal[7]) -> bool { return is_5(literal) && *command.nth(5) == literal[5]; };
            static const auto is_7 = [=](const char literal[8]) -> bool { return is_6(literal) && *command.nth(6) == literal[6]; };

            if (command) {
                recv_buf.complete_read(command.length() + 1); // eat command name + space
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



        inline bytes protocol_parser::parse_header(io_buffer & recv_buf) {
            auto unread = bytes(recv_buf.begin_read(), recv_buf.non_read());
            auto header_end = unread.search(CRLF);
            if (header_end) {
                bytes header, rest;
                tie(header, rest) = unread.split_at(header_end.begin());
                recv_buf.complete_read(header.length() + CRLF.length());
                return header;
            } else {
                throw system_error(error::not_enough_data);
            }
        }

        inline bytes protocol_parser::parse_retrieval_command(io_buffer & recv_buf) {
            return parse_header(recv_buf);
        }


        inline tuple<bytes, bytes> protocol_parser::parse_retrieval_command_keys(bytes keys) {
            bytes first, rest;
            tie(first, rest) = keys.split(SPACE);
            validate_key(first);
            return make_tuple(first, rest);
        }


        /// @return `true` if buffer equals `noreply`
        inline bool protocol_parser::maybe_noreply(const bytes buffer) {
            if (buffer.empty()) {
                return false;
            } else if (buffer == NOREPLY) {
                return true;
            } else {
                throw system_error(error::noreply_expected);
            }
        }

        
        /// Parse args of on of the: `ADD`, `SET`, `REPLACE`, `CAS`, `APPEND`, `PREPEND` commands
        inline tuple<bytes, bytes, cache::opaque_flags_type, cache::expiration_time_point, cache::version_type, bool> protocol_parser::parse_storage_command(cache::Command cmd, io_buffer & recv_buf) {
            auto args = parse_header(recv_buf);
            bytes key;
            tie(key, args) = args.split(SPACE);
            validate_key(key);
            bytes parsed;
            tie(parsed, args) = args.split(SPACE);
            cache::opaque_flags_type flags = str_to_int<cache::opaque_flags_type>(parsed.begin(), parsed.end());
            tie(parsed, args) = args.split(SPACE);
            auto keep_alive_duration = cache::seconds(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
            tie(parsed, args) = args.split(SPACE);
            uint32 datalen = str_to_int<uint32>(parsed.begin(), parsed.end());
            if (datalen > settings.cache.max_value_size) {
                throw system_error(error::value_length);
            }
            cache::version_type cas_unique = 0;
            if (cmd == cache::CAS) {
                tie(parsed, args) = args.split(SPACE);
                cas_unique = str_to_int<cache::version_type>(parsed.begin(), parsed.end());
            }
            bool noreply = maybe_noreply(args);
            if (recv_buf.non_read() >= datalen + CRLF.length()) {
                auto value = bytes(recv_buf.begin_read(), datalen);
                recv_buf.complete_read(datalen);
                if (bytes(recv_buf.begin_read(), CRLF.length()) == CRLF) {
                    recv_buf.complete_read(CRLF.length());
                } else {
                    throw system_error(error::crlf_expected);
                }
                // TODO: Memcached at some point treats `keep_alive_duration` as UNIX epoch time
                return cachelot::make_tuple(key, value, flags, expiration_time_point(keep_alive_duration), cas_unique, noreply);
            } else {
                throw system_error(error::not_enough_data);
            }
        }


        /// Parse the `DELETE` command args
        inline tuple<bytes, bool> protocol_parser::parse_delete_command(io_buffer & recv_buf) {
            auto args = parse_header(recv_buf);
            bytes key;
            tie(key, args) = args.split(SPACE);
            validate_key(key);
            bool noreply = maybe_noreply(args);
            return cachelot::make_tuple(key, noreply);
        }


        /// Parse the `INCR` and `DECR` command args
        inline tuple<bytes, uint64, bool> protocol_parser::parse_arithmetic_command(io_buffer & recv_buf) {
            auto args = parse_header(recv_buf);
            bytes key;
            tie(key, args) = args.split(SPACE);
            validate_key(key);
            bytes parsed;
            tie(parsed, args) = args.split(SPACE);
            auto delta = str_to_int<uint64>(parsed.begin(), parsed.end());
            bool noreply = maybe_noreply(args);
            return cachelot::make_tuple(key, delta, noreply);
        }


        /// Parse the `TOUCH` command args
        inline tuple<bytes, cache::expiration_time_point, bool> protocol_parser::parse_touch_command(io_buffer & recv_buf) {
            auto args = parse_header(recv_buf);
            bytes key;
            tie(key, args) = args.split(SPACE);
            validate_key(key);
            bytes parsed;
            tie(parsed, args) = args.split(SPACE);
            error_code error;
            cache::seconds keep_alive_duration(str_to_int<cache::seconds::rep>(parsed.begin(), parsed.end()));
            bool noreply = maybe_noreply(args);
            return cachelot::make_tuple(key, expiration_time_point(keep_alive_duration), noreply);
        }


        /// Parse the `STATS` command args
        inline bytes protocol_parser::parse_statistics_command(io_buffer & recv_buf) {
            return parse_header(recv_buf); // TODO: Not Implemented
        }


        /// Stream operator to serialize `bytes`
        inline io_buffer & operator<<(io_buffer & buf, const bytes value) {
            auto dest = buf.begin_write(value.length());
            std::memcpy(dest, value.begin(), value.length());
            buf.complete_write(value.length());
            return buf;
        }


        /// Stream operator to serialize single `char`
        inline io_buffer & operator<<(io_buffer & buf, const char value) {
            auto dest = buf.begin_write(sizeof(char));
            *dest = value;
            buf.complete_write(sizeof(char));
            return buf;
        }


        /// Stream operator to serialize cache response as an ascii string
        inline io_buffer & operator<<(io_buffer & buf, cache::Response resp) {
            #define CACHE_RESPONSES_ENUM_STRELEMENT(resp) bytes::from_literal(CACHELOT_PP_STR(resp)),
            constexpr bytes __AsciiResponses[] = {
                CACHE_RESPONSES_ENUM(CACHE_RESPONSES_ENUM_STRELEMENT)
            };
            #undef CACHE_RESPONSES_ENUM_STRELEMENT
            return buf << __AsciiResponses[static_cast<unsigned>(resp)];
        }


        /// Heler function to write integer into io_buffer as an ascii string
        template<typename IntType>
        inline io_buffer & serialize_integer(io_buffer & buf, const IntType x) {
            auto dest = buf.begin_write(internal::numeric<IntType>::max_str_length);
            size_t written = int_to_str(x, dest);
            buf.complete_write(written);
            return buf;
        }
        #define __do_serialize_integer_ascii(IntType) \
        inline io_buffer & operator<<(io_buffer & buf, const IntType value) { \
            return serialize_integer<IntType>(buf, value); \
        }
        /// Stream operator to serialize `uint16` integer as an ascii string
        __do_serialize_integer_ascii(uint16)
        /// Stream operator to serialize `uint32` integer as an ascii string
        __do_serialize_integer_ascii(uint32)
        /// Stream operator to serialize `uint64` integer as an ascii string
        __do_serialize_integer_ascii(uint64)

        #undef __do_serialize_integer_ascii


        /// Serialize cache Item into the io_buffer
        inline void protocol_parser::write_item(io_buffer & buf, cache::ItemPtr item, bool with_cas) {
            buf << VALUE << SPACE << item->key() << SPACE << item->opaque_flags() << SPACE << static_cast<uint32>(item->value().length());
            if (with_cas) {
                buf << SPACE << item->version();
            }
            buf << CRLF << item->value() << CRLF;
        }


        inline void protocol_parser::finalize_batch(io_buffer & send_buf) {
            send_buf << CRLF;
        }


        inline void protocol_parser::write_response(io_buffer & send_buf, cache::Response response) {
            send_buf << response << CRLF;
        }


        inline void protocol_parser::write_unknown_command_error(io_buffer & send_buf) {
            send_buf << ERROR << CRLF;
        }


        inline void protocol_parser::write_client_error(io_buffer & send_buf, bytes message) {
            send_buf << CLIENT_ERROR << message << CRLF;
        }


        inline void protocol_parser::write_server_error(io_buffer & send_buf, bytes message) {
            send_buf << SERVER_ERROR << message << CRLF;
        }

    }} // namespace memcached::ascii

} // namespace cachelot

/// @}

#endif // CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED

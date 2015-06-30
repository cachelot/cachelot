#ifndef CACHELOT_PROTO_MEMCACHED_H_INCLUDED
#define CACHELOT_PROTO_MEMCACHED_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif
#ifndef CACHELOT_NET_STREAM_CONNECTION_H_INCLUDED
#  include <server/stream_connection.h>
#endif
#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#  include <server/io_buffer.h>
#endif
#ifndef CACHELOT_IO_SERIALIZATION_H_INCLUDED
#  include <server/io_serialization.h>
#endif
#ifndef CACHELOT_CACHE_H_INCLUDED
#  include <cachelot/cache.h>
#endif
#ifndef CACHELOT_SETTINGS_H_INCLUDED
#  include <server/settings.h>
#endif


/// @defgroup memcached Memcached protocols implementation
/// @{

namespace cachelot {

    /// @ref memcached
    namespace memcached {

        /// Memcached protocol-related errors
        #define MEMCACHED_PROTO_ERROR_ENUM(x)                                       \
            x(not_enough_data, "Expected more data in the message")                 \
            x(key_length, "Maximum key length exceeded")                            \
            x(value_length, "Maximum value length exceeded")                        \
            x(crlf_expected, "Invalid request: \\r\\n expected")                    \
            x(value_crlf_expected, "Invalid value: \\r\\n expected")                \
            x(key_expected, "Invalid request: key expected")                        \
            x(integer_conv, "Invalid request: failed to convert integer argument")  \
            x(integer_range, "Invalid request: integer value is out of range")      \
            x(noreply_expected, "Invalid request: expected noreply")                \
            x(udp_header_size, "UDP packet header is too small")                    \
            x(udp_proto_reserverd, "UDP reserved flag expected to be zero")

        /// UDP Protocol header
        struct udp_header {
            static constexpr size_t size = 8;
            uint16 request_id;
            uint16 sequence_no;
            uint16 packets_in_msg;
            uint16 reserved;

            static udp_header fromBytes(bytes raw);
        };

        /// memcached protocol parser interface
        struct basic_protocol_parser {
            static basic_protocol_parser & determine_protocol(bytes data);

            /// ctor
            basic_protocol_parser() = default;

            /// dtor
            virtual ~basic_protocol_parser() {}

            /// Parse the name of the `command`
            virtual cache::Command parse_command_name(const bytes command) = 0;

            /// Parse the first key of `GET` and `GETS` commans
            /// @return the key and the rest args
            virtual tuple<bytes, bytes> parse_retrieval_command_key(bytes args) = 0;

            /// Parse args of on of the: `ADD`, `SET`, `REPLACE`, `CAS`, `APPEND`, `PREPEND` commands
            virtual tuple<bytes, cache::opaque_flags_type, cache::expiration_time_point, uint32, cache::version_type, bool> parse_storage_command(cache::Command cmd, bytes args) = 0;

            /// Parse the `DELETE` command args
            virtual tuple<bytes, bool> parse_delete_command(bytes args) = 0;

            /// Parse the `INCR` and `DECR` command args
            virtual tuple<bytes, uint64, bool> parse_arithmetic_command(bytes args) = 0;

            /// Parse the `TOUCH` command args
            virtual tuple<bytes, cache::expiration_time_point, bool> parse_touch_command(bytes args) = 0;

            /// Parse the `STATS` command args
            virtual bytes parse_statistics_command(bytes args) = 0;

            /// Serialize cache Item into the io_buffer
            virtual void write_item(io_buffer & buf, cache::ItemPtr item, bool with_cas) = 0;

            /// Serialize one of the cache responses
            virtual void write_response(io_buffer & buf, cache::Response response) = 0;

            /// Serialize unknown command error
            virtual void write_unknown_command_error(io_buffer & buf) = 0;

            /// Serialize client error
            virtual void write_client_error(io_buffer & buf, bytes message) = 0;

            /// Serialize server error
            virtual void write_server_error(io_buffer & buf, bytes message) = 0;
        };

        namespace error {

            enum protocol_error_code {
                #define MEMCACHED_PROTO_ERROR_ENUM_CODE(code, _) code,
                MEMCACHED_PROTO_ERROR_ENUM(MEMCACHED_PROTO_ERROR_ENUM_CODE)
                #undef MEMCACHED_PROTO_ERROR_ENUM_CODE
            };


            class memcached_protocol_error_category : public error_category {
            public:
                memcached_protocol_error_category() = default;

                virtual const char * name() const noexcept override { return "Memcached protocol error"; }

                virtual string message(int value) const override {
                    #define MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG(code, msg) case code: return msg;
                    switch (static_cast<protocol_error_code>(value)) {
                        MEMCACHED_PROTO_ERROR_ENUM(MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG)
                        default: return "Invalid error code";
                    }
                    #undef MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG
                }
            };

            inline const error_category & get_protocol_error_category() noexcept {
                static error::memcached_protocol_error_category category_instance;
                return category_instance;
            }

            inline error_code make_error_code(error::protocol_error_code ec) noexcept {
                return error_code(static_cast<int>(ec), get_protocol_error_category());
            }
        }


        inline const error_category & get_protocol_error_category() noexcept {
            return error::get_protocol_error_category();
        }

        /// validate the Item key
        inline void validate_key(const bytes key) {
            if (not key) {
                throw system_error(error::make_error_code(error::key_expected));
            }
            if (key.length() > cache::Item::max_key_length) {
                throw system_error(error::make_error_code(error::key_length));
            }
        }


        /// Convert expiration duration to the expiration time point from now
        inline cache::clock::time_point expiration_time_point(cache::seconds keep_alive_duration) {
            if (keep_alive_duration == cache::seconds(0)) {
                return cache::expiration_time_point::max();
            } else {
                return cache::clock::now() + keep_alive_duration;
            }
        }

        inline udp_header udp_header::fromBytes(bytes raw) {
            debug_assert(unaligned_bytes(raw.begin(), alignof(uint16)) == 0);
            if (raw.length() < udp_header::size) {
                throw system_error(error::make_error_code(error::udp_header_size));
            }
            uint16 request_id = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 0));
            uint16 sequence_no = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 1));
            uint16 packets_in_msg = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 2));
            uint16 reserved = *reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 3);
            if (reserved != 0) {
                throw system_error(error::make_error_code(error::udp_proto_reserverd));
            }
            return udp_header { request_id, sequence_no, packets_in_msg, reserved };
        }

        

} } // namespace cachelot::memcached

/// @}

namespace boost { namespace system {
    // register error_code type
    template <> struct is_error_code_enum<cachelot::memcached::error::protocol_error_code> {
        static const bool value = true;
    };
} } // namespace boost::system

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

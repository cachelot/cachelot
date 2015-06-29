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
#ifndef CACHELOT_NET_CONN_STREAM_H_INCLUDED
#  include <server/conn_stream.h>
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

        /// Classes of memcached errors
        constexpr bytes ERROR = bytes::from_literal("ERROR"); ///< unknown command
        constexpr bytes CLIENT_ERROR = bytes::from_literal("CLIENT_ERROR"); ///< request is ill-formed
        constexpr bytes SERVER_ERROR = bytes::from_literal("SERVER_ERROR"); ///< internal server error or out-of-memory

        /// Memcached protocol-related errors
        #define MEMCACHED_PROTO_ERROR_ENUM(x)                                       \
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
        }


        inline const error_category & get_protocol_error_category() noexcept {
            static error::memcached_protocol_error_category category_instance;
            return category_instance;
        }


        inline error_code make_protocol_error(error::protocol_error_code ec) noexcept {
            return error_code(static_cast<int>(ec), get_protocol_error_category());
        }

        /// validate the Item key
        inline void validate_key(const bytes key) {
            if (not key) {
                throw system_error(make_protocol_error(error::key_expected));
            }
            if (key.length() > cache::Item::max_key_length) {
                throw system_error(make_protocol_error(error::key_length));
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
                throw system_error(make_protocol_error(error::udp_header_size));
            }
            uint16 request_id = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 0));
            uint16 sequence_no = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 1));
            uint16 packets_in_msg = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 2));
            uint16 reserved = *reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 3);
            if (reserved != 0) {
                throw system_error(make_protocol_error(error::udp_proto_reserverd));
            }
            return udp_header { request_id, sequence_no, packets_in_msg, reserved };
        }

} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

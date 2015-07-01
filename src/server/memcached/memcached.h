#ifndef CACHELOT_PROTO_MEMCACHED_H_INCLUDED
#define CACHELOT_PROTO_MEMCACHED_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


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
#ifndef CACHELOT_MEMCACHED_ERROR_H_INCLUDED
#  include <server/memcached/error.h>
#endif


/// @defgroup memcached Memcached protocols implementation
/// @{

namespace cachelot {

    /// @ref memcached
    namespace memcached {

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
            /// choose between ascii and binary protocol
            static std::unique_ptr<basic_protocol_parser> determine_protocol(io_buffer & recv_buf);

            /// ctor
            basic_protocol_parser() = default;

            /// dtor
            virtual ~basic_protocol_parser() {}

            /// Parse the name of the `command`
            virtual cache::Command parse_command_name(io_buffer & recv_buf) = 0;

            /// Parse the `GET` or `GETS` command header
            virtual bytes parse_retrieval_command(io_buffer & recv_buf) = 0;

            /// Parse the first key of `GET` and `GETS` commans
            /// @return the key and the rest args
            virtual tuple<bytes, bytes> parse_retrieval_command_keys(bytes keys) = 0;

            /// Parse args of on of the: `ADD`, `SET`, `REPLACE`, `CAS`, `APPEND`, `PREPEND` commands
            virtual tuple<bytes, bytes, cache::opaque_flags_type, cache::expiration_time_point, cache::version_type, bool> parse_storage_command(cache::Command cmd, io_buffer & recv_buf) = 0;

            /// Parse the `DELETE` command args
            virtual tuple<bytes, bool> parse_delete_command(io_buffer & recv_buf) = 0;

            /// Parse the `INCR` and `DECR` command args
            virtual tuple<bytes, uint64, bool> parse_arithmetic_command(io_buffer & recv_buf) = 0;

            /// Parse the `TOUCH` command args
            virtual tuple<bytes, cache::expiration_time_point, bool> parse_touch_command(io_buffer & recv_buf) = 0;

            /// Parse the `STATS` command args
            virtual bytes parse_statistics_command(io_buffer & recv_buf) = 0;

            /// Serialize cache Item into the io_buffer
            virtual void write_item(io_buffer & send_buf, cache::ItemPtr item, bool with_cas) = 0;

            /// Finalize the batch
            virtual void finalize_batch(io_buffer & send_buf) = 0;

            /// Serialize one of the cache responses
            virtual void write_response(io_buffer & send_buf, cache::Response response) = 0;

            /// Serialize unknown command error
            virtual void write_unknown_command_error(io_buffer & send_buf) = 0;

            /// Serialize client error
            virtual void write_client_error(io_buffer & send_buf, bytes message) = 0;

            /// Serialize server error
            virtual void write_server_error(io_buffer & send_buf, bytes message) = 0;
        };


        /// validate the Item key
        inline void validate_key(const bytes key) {
            if (not key) {
                throw system_error(error::key_expected);
            }
            if (key.length() > cache::Item::max_key_length) {
                throw system_error(error::key_length);
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

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

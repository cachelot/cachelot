#ifndef CACHELOT_SERVER_MEMCACHED_H_INCLUDED
#define CACHELOT_SERVER_MEMCACHED_H_INCLUDED

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


        class basic_protocol_handler {
        public:
            /// constructor
            explicit basic_protocol_handler(cache::Cache & the_cache) : cache_api(the_cache), calc_hash() {}

            /// virtual destructor
            virtual ~basic_protocol_handler() = default;

            /// React on the data
            virtual net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept = 0;

            /// choose between ascii and binary protocol
            static std::unique_ptr<basic_protocol_handler> determine_protocol(io_buffer & recv_buf, cache::Cache & the_cache);

        protected:
            cache::Cache & cache_api;
            const cache::HashFunction calc_hash;
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

#endif // CACHELOT_SERVER_MEMCACHED_H_INCLUDED

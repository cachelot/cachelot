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

        /// Process every received packet
        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api);

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

} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_SERVER_MEMCACHED_H_INCLUDED

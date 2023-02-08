#ifndef CACHELOT_SERVER_MEMCACHED_H_INCLUDED
#define CACHELOT_SERVER_MEMCACHED_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_SLICE_H_INCLUDED
#  include <cachelot/slice.h>
#endif
#ifndef CACHELOT_NET_SOCKET_STREAM_H_INCLUDED
#  include <server/socket_stream.h>
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
#ifndef CACHELOT_MEMCACHED_PROTO_DEFS_H_INCLUDED
#  include <server/memcached/proto_defs.h>
#endif



namespace cachelot {

    /// @defgroup memcached Memcached protocols implementation
    /// @{
    namespace memcached {

        /// Process every received packet
        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api);

        /// validate the Item key
        inline void validate_key(const slice key) {
            if (not key) {
                throw system_error(error::key_expected);
            }
            if (key.length() > cache::Item::max_key_length) {
                throw system_error(error::key_length);
            }
        }

    } // namespace memcached

    /// @}

} // namespace cachelot

#endif // CACHELOT_SERVER_MEMCACHED_H_INCLUDED

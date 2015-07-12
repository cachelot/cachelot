#ifndef CACHELOT_PROTO_BINARY_H_INCLUDED
#define CACHELOT_PROTO_BINARY_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <server/memcached/memcached.h>


/// @ingroup memcached
/// @{

namespace cachelot {

    namespace memcached { namespace binary {

        /// Identifier of the binary protocol (request packet identifier)
        const extern uint8 MAGIC;

        /// Main function that process binary protocol packets
        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api);

    }} // namespace memcached::binary

} // namespace cachelot

/// @}

#endif // CACHELOT_PROTO_BINARY_H_INCLUDED

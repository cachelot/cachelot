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

        /// Main function that process ascii protocol packets
        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api) noexcept;

    }} // namespace memcached::ascii

} // namespace cachelot

/// @}

#endif // CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED

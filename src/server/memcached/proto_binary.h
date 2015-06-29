#ifndef CACHELOT_PROTO_BINARY_H_INCLUDED
#define CACHELOT_PROTO_BINARY_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <server/memcached/memcached.h>
#include <memcached/protocol_binary.h>


/// @ingroup memcached
/// @{

namespace cachelot {

    namespace memcached { namespace binary {

        constexpr auto MAGIC = PROTOCOL_BINARY_REQ;

    }} // namespace memcached::binary

} // namespace cachelot

/// @}

#endif // CACHELOT_PROTO_BINARY_H_INCLUDED

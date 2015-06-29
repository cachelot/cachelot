#ifndef CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED
#define CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup net
/// @{

namespace cachelot {

    namespace net {

        template <class ImplType, class ConnType>
        class datagram_server {
            using typename ConnType::socket_type;
        };

    } // namespace net

} // namespace cachelot

/// @}

#endif // CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED

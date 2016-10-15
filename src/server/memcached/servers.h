#ifndef CACHELOT_PROTO_MEMCACHED_SERVERS_H_INCLUDED
#define CACHELOT_PROTO_MEMCACHED_SERVERS_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <server/stream_server.h>
#include <server/datagram_server.h>
#include <server/memcached/proto_text.h>


namespace cachelot {

    namespace memcached {

    /// @addtogroup memcached
    /// @{

    using namespace cachelot::net;

    /// text protocol handler on TCP socket
    typedef text_protocol_handler<tcp::socket> tcp_text_protocol_handler;

    /// TCP based memcached server handling text protocol
    class text_tcp_server : public stream_server<text_tcp_server, tcp_text_protocol_handler> {
        typedef stream_server<text_tcp_server, tcp_text_protocol_handler> super;
    public:
        text_tcp_server(io_service & ios, cache::Cache & the_cache)
            : super(ios)
            , cache(the_cache) {
        }

        ~text_tcp_server() {
            super::stop();
        }

        tcp_text_protocol_handler * new_connection() {
            return tcp_text_protocol_handler::create(super::get_io_service(), cache);
        }

        void delete_connection(tcp_text_protocol_handler * conn) noexcept {
            tcp_text_protocol_handler::destroy(conn);
        }

    private:
        cache::Cache & cache;
    };


    /// text protocol handler on unix stream socket
    typedef text_protocol_handler<local::stream_protocol::socket> unix_text_protocol_handler;

    /// Unix socket based memcached server handling text protocol
    class text_local_socket_server : public stream_server<text_local_socket_server, unix_text_protocol_handler> {
        typedef stream_server<text_local_socket_server, unix_text_protocol_handler> super;
    public:
        explicit text_local_socket_server(asio::io_service & ios, cache::Cache & the_cache)
            : super(ios)
            , cache(the_cache) {
        }

        ~text_local_socket_server() {
            super::stop();
        }

        unix_text_protocol_handler * new_connection() {
            return unix_text_protocol_handler::create(super::get_io_service(), cache);
        }

        void delete_connection(unix_text_protocol_handler * conn) noexcept {
            debug_assert(conn);
            unix_text_protocol_handler::destroy(conn);
        }

    private:
        cache::Cache & cache;
    };

    /// @}

    } // namespace memcached

} // namespace cachelot


#endif // CACHELOT_PROTO_MEMCACHED_SERVERS_H_INCLUDED


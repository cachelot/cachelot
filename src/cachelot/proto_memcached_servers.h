#ifndef CACHELOT_MEMCACHED_TCP_H_INCLUDED
#define CACHELOT_MEMCACHED_TCP_H_INCLUDED

#include <cachelot/tcp_server.h>
#include <cachelot/unix_socket_server.h>
#include <cachelot/proto_memcached_text.h>

/**
 * @ingroup memcached
 * @{
 */

namespace cachelot {

    namespace memcached {

    using namespace cachelot::net;

    /// text protocol handler on TCP socket
    typedef text_protocol_handler<tcp::socket> tcp_text_protocol_handler;

    /// TCP based memcached server handling text protocol
    class text_tcp_server : public tcp_server<text_tcp_server, tcp_text_protocol_handler> {
        typedef tcp_server<text_tcp_server, tcp_text_protocol_handler> super;
    public:
        text_tcp_server(io_service & ios, cache::CacheService & the_cache)
            : super(ios)
            , cache(the_cache) {
        }

        tcp_text_protocol_handler * new_connection() {
            return tcp_text_protocol_handler::create(*(new io_service()), cache.shared_from_this());
        }

        void delete_connection(tcp_text_protocol_handler * conn) noexcept {
            tcp_text_protocol_handler::destroy(conn);
        }

    private:
        cache::CacheService & cache;
    };


    /// text protocol handler on unix stream socket
    typedef text_protocol_handler<local::stream_protocol::socket> unix_text_protocol_handler;

    /// Unix socket based memcached server handling text protocol
    class text_unix_server : public unix_stream_server<text_unix_server, unix_text_protocol_handler> {
        typedef unix_stream_server<text_unix_server, unix_text_protocol_handler> super;
    public:
        explicit text_unix_server(asio::io_service & ios, std::shared_ptr<Actor> the_cache)
            : super(ios)
            , cache(the_cache) {
        }

        unix_text_protocol_handler * new_connection() {
            return unix_text_protocol_handler::create(ios(), cache);
        }

        void delete_connection(unix_text_protocol_handler * conn) noexcept {
            debug_assert(conn);
            unix_text_protocol_handler::destroy(conn);
        }

    private:
        std::shared_ptr<Actor> cache;
    };

} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_MEMCACHED_UNIX_H_INCLUDED

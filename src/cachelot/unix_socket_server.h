#ifndef CACHELOT_NET_UNIX_SOCKET_SERVER_H_INCLUDED
#define CACHELOT_NET_UNIX_SOCKET_SERVER_H_INCLUDED

#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <cachelot/network.h>
#endif

/**
 * @ingroup net
 * @{
 */

namespace cachelot { namespace net {

    /**
     * @brief tcp_server is a server for TCP/IP protocol family
     */
    template <class Impl, class ConnectionType>
    class unix_stream_server {
    public:
        /// constructor
        explicit unix_stream_server(io_service & ios)
            : m_ios(ios)
            , m_acceptor(ios) {
        }

        unix_stream_server(const unix_stream_server &) = delete;
        unix_stream_server & operator= (const unix_stream_server &) = delete;

        /// start accept connections
        void start(local::stream_protocol::endpoint);

        /// start accept connections
        void start(const string socket_name) {
            start(local::stream_protocol::endpoint(socket_name));
        }

        /// interrupt all activity
        void stop() {
            error_code __;
            m_acceptor.close(__);
        }

        io_service & get_io_service() noexcept { return m_ios; }

    private:
        void async_accept();
        void handle_accept(const error_code & error, ConnectionType * connection);

    private:
        io_service & m_ios;
        local::stream_protocol::acceptor m_acceptor;
    };

    template <class Impl, class ConnectionType>
    inline void unix_stream_server<Impl, ConnectionType>::start(const local::stream_protocol::endpoint bind_addr) {
        m_acceptor.open();
        m_acceptor.bind(bind_addr);
        m_acceptor.listen();
        async_accept();
    }

    template <class Impl, class ConnectionType>
    inline void unix_stream_server<Impl, ConnectionType>::async_accept() {
        ConnectionType * new_connection = reinterpret_cast<Impl *>(this)->new_connection();
        m_acceptor.async_accept(new_connection->socket(),
            [=](const error_code error) {
                net_debug_trace("[error: %s]", error.message().c_str());
                if (!error) {
                    this->async_accept();
                    new_connection->run();
                } else {
                    reinterpret_cast<Impl *>(this)->delete_connection(new_connection);
                    this->stop();
                }

            });
    }


}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NET_UNIX_SOCKET_SERVER_H_INCLUDED

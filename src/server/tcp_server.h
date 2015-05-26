#ifndef CACHELOT_NET_TCP_SERVER_H_INCLUDED
#define CACHELOT_NET_TCP_SERVER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <server/network.h>
#endif

/**
 * @ingroup net
 * @{
 */

namespace cachelot { namespace net {

    /**
     * tcp_server is a server for TCP/IP protocol family
     *
     * @tparam Impl -           Actual TCP Server implementation class, must be derived from tcp_server
     *                          tcp_server expects that Impl class provides following functions:
     *                          `ConnectionType * new_connection()` to create new connections
     *                          `delete_connection(ConnectionType *)` to delete broken connections
     * @tparam ConnectionType - class representing connections of this TCP Server
     *                          tcp_server expects that ConnectionType provides function
     *                          `void run()` that will be called after establishing connection
     *                           with the client
     */
    template <class Impl, class ConnectionType>
    class tcp_server {
        typedef tcp_server<Impl, ConnectionType> this_type;
    public:
        /// constructor
        explicit tcp_server(io_service & ios)
            : m_ios(ios)
            , m_acceptor(ios) {
            static_assert(std::is_base_of<this_type, Impl>::value, "TCP Server implementation must be derived from tcp_server");
            tcp::acceptor::reuse_address reuse_address_option(true); error_code ignore_error;
            m_acceptor.set_option(reuse_address_option, ignore_error);
        }

        tcp_server(const tcp_server &) = delete;
        tcp_server & operator= (const tcp_server &) = delete;

        /// start accept connections
        void start(const tcp::endpoint bind_addr);

        /// start accept connections
        void start(uint16 port) {
            tcp::endpoint bind_addr(ip::address_v4::any(), port);
            start(bind_addr);
        }

        /// interrupt all activity
        void stop() noexcept {
            error_code __;
            m_acceptor.close(__);
        }

        io_service & get_io_service() noexcept { return m_ios; }

    private:
        void async_accept();

    private:
        io_service & m_ios;
        tcp::acceptor m_acceptor;
    };

    template <class Impl, class ConnectionType>
    inline void tcp_server<Impl, ConnectionType>::start(const tcp::endpoint bind_addr) {
        m_acceptor.open(tcp::v4());
        m_acceptor.bind(bind_addr);
        m_acceptor.listen();
        async_accept();
    }

    template <class Impl, class ConnectionType>
    inline void tcp_server<Impl, ConnectionType>::async_accept() {
        try {
            ConnectionType * new_connection = reinterpret_cast<Impl *>(this)->new_connection();
            m_acceptor.async_accept(new_connection->socket(),
                [=](const error_code error) {
                    if (not error) {
                        new_connection->run();
                    } else {
                        reinterpret_cast<Impl *>(this)->delete_connection(new_connection);
                    }
                    this->async_accept();
                });
        } catch (const std::bad_alloc &) {
            // retry later TODO: will m_ios.post throw ???
            m_ios.post([=](){ this->async_accept(); });
        }
    }


}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NET_TCP_SERVER_H_INCLUDED

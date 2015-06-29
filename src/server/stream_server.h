#ifndef CACHELOT_NET_STREAM_SERVER_H_INCLUDED
#define CACHELOT_NET_STREAM_SERVER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <server/network.h>
#endif


/// @ingroup net
/// @{

namespace cachelot {

    namespace net {

        /**
         * stream_server is an acceptor and connection manager for the `SOCK_STREAM` sockets (TCP/IP and local unix stream socket)
         *
         * @tparam ImplType -       Actual protocol_type Server implementation class, must be derived from stream_server
         *                          stream_server expects that ImplType class provides the following functions:
         *                          `ConnectionType * new_connection()` to create new connections
         *                          `delete_connection(ConnectionType *)` to delete broken / expired connections
         * @tparam ConnectionType - class representing connections of this protocol_type Server
         *                          stream_server expects that ConnectionType provides function
         *                          `void run()` that will be called after establishing connection
         *                           with the client
         */
        template <class ImplType, class ConnectionType>
        class stream_server {
            typedef stream_server<ImplType, ConnectionType> this_type;
            typedef typename ConnectionType::socket_type socket_type;
            typedef typename socket_type::protocol_type protocol_type;
        public:
            /// constructor
            explicit stream_server(io_service & ios)
                : m_ios(ios)
                , m_acceptor(ios) {
                static_assert(std::is_base_of<this_type, ImplType>::value, "Server implementation <ImplType> must be derived from stream_server");
                typename protocol_type::acceptor::reuse_address reuse_address_option(true); error_code ignore_error;
                m_acceptor.set_option(reuse_address_option, ignore_error);
            }

            stream_server(const stream_server &) = delete;
            stream_server & operator= (const stream_server &) = delete;

            /// start accept connections
            void start(const typename protocol_type::endpoint bind_addr);

            /// start accept connections
            void start(uint16 port) {
                typename protocol_type::endpoint bind_addr(ip::address_v4::any(), port);
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
            typename protocol_type::acceptor m_acceptor;
        };

        template <class ImplType, class ConnectionType>
        inline void stream_server<ImplType, ConnectionType>::start(const typename protocol_type::endpoint bind_addr) {
            m_acceptor.open(bind_addr.protocol());
            m_acceptor.bind(bind_addr);
            m_acceptor.listen();
            async_accept();
        }

        template <class ImplType, class ConnectionType>
        inline void stream_server<ImplType, ConnectionType>::async_accept() {
            try {
                ConnectionType * new_connection = reinterpret_cast<ImplType *>(this)->new_connection();
                m_acceptor.async_accept(new_connection->socket(),
                    [=](const error_code error) {
                        if (not error) {
                            new_connection->run();
                        } else {
                            reinterpret_cast<ImplType *>(this)->delete_connection(new_connection);
                        }
                        this->async_accept();
                    });
            } catch (const std::bad_alloc &) {
                // retry later TODO: will m_ios.post throw ???
                m_ios.post([=](){ this->async_accept(); });
            }
        }

    } // namespace net

} // namespace cachelot

/// @}

#endif // CACHELOT_NET_STREAM_SERVER_H_INCLUDED

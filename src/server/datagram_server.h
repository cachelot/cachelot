#ifndef CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED
#define CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <server/network.h>
#endif
#ifndef CACHELOT_DICT_H_INCLUDED
#  include <cachelot/dict.h>
#endif

/// @ingroup net
/// @{

namespace cachelot {

    namespace net {

        /**
         * datagram_server is an acceptor and connection manager for the `SOCK_DGRAM` sockets (UDP/IP and local unix datagram socket)
         * @see stream_server documentation for the template parameters explanation
         */
        template <class ImplType, class ConnectionType>
        class datagram_server {
            typedef datagram_server<ImplType, ConnectionType> this_type;
            typedef typename ConnectionType::socket_type socket_type;
            typedef typename socket_type::protocol_type protocol_type;
        public:
            /// constructor
            explicit datagram_server(io_service & ios)
                : m_ios(ios)
                , m_socket(ios)
                , m_recv_buf(default_min_buffer_size, default_max_buffer_size) {
                static_assert(std::is_base_of<this_type, ImplType>::value, "<ImplType> must be derived from the datagram_server");
            }

            datagram_server(const datagram_server &) = delete;
            datagram_server & operator= (const datagram_server &) = delete;

            /// start accept connections
            void start(const typename protocol_type::endpoint bind_addr);

            /// interrupt all activity
            void stop() noexcept {
                error_code __;
                m_socket.close(__);
            }

            io_service & get_io_service() noexcept { return m_ios; }

        private:
            void start_receive();

            void handle_data(bytes data, const typename protocol_type::endpoint remote_addr) noexcept;

        private:
            io_service & m_ios;
            socket_type m_socket;
            io_buffer m_recv_buf;
            dict<typename protocol_type::endpoint, std::unique_ptr<ConnectionType>> m_conn_by_endpoint;
        };

        template <class ImplType, class ConnectionType>
        inline void datagram_server<ImplType, ConnectionType>::start(const typename protocol_type::endpoint bind_addr) {
            m_socket.open(bind_addr.protocol());
            error_code ignore_error;
            m_socket.set_option(typename socket_type::reuse_address(true), ignore_error);
            m_socket.bind(bind_addr);
            start_receive();
        }

        template <class ImplType, class ConnectionType>
        inline void datagram_server<ImplType, ConnectionType>::start_receive() {
            typename protocol_type::endpoint remote_endpoint;
            m_socket.async_receive_from(asio::buffer(m_recv_buf.begin_write(), m_recv_buf.available()), remote_endpoint,
                 [=] (const error_code error, size_t bytes_recvd) noexcept {
                     if (not error) {
                         m_recv_buf.complete_write(bytes_recvd);
                         handle_data(m_recv_buf.read_all(), remote_endpoint);
                     } else {
                         m_recv_buf.discard_all();
                     }
                     this->start_receive();
                 });
        }

    } // namespace net

} // namespace cachelot

/// @}

#endif // CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED

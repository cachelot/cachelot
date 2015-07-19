#ifndef CACHELOT_NET_SOCKET_DATAGRAM_H_INCLUDED
#define CACHELOT_NET_SOCKET_DATAGRAM_H_INCLUDED

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
         * 
         * @tparam SocketType is a datagram socket from the boost::asio library
         * @tparam ConversationType is an implementation of the protocol handler
         */
        template <class SocketType>
        class datagram_server {
            typedef SocketType socket_type;
            typedef typename socket_type::protocol_type protocol_type;
        public:
            /// constructor
            explicit datagram_server(io_service & ios)
                : m_ios(ios)
                , m_socket(ios)
                , m_recv_buf(default_min_buffer_size, default_max_buffer_size)
                , m_send_buf(default_min_buffer_size, default_max_buffer_size) {
            }

            /// virtual destructor
            virtual ~datagram_server() = default;

            datagram_server(const datagram_server &) = delete;
            datagram_server & operator= (const datagram_server &) = delete;

            /// start accept connections
            void start(const typename protocol_type::endpoint bind_addr);

            /// interrupt all activity
            void close() noexcept {
                error_code ignore_error;
                m_socket.close(ignore_error);
            }

            io_service & get_io_service() noexcept { return m_ios; }

        protected:
            /// React on the data
            /// Parse incoming message from the `recv_buf`, call the Cache API functions and write reply into the `send_buf`
            /// @return ConversationReply indicates whether to send reply or just wait for more data
            virtual net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept = 0;

        private:
            void async_receive() noexcept;
            void async_send_all(const typename protocol_type::endpoint to) noexcept;

        private:
            io_service & m_ios;
            socket_type m_socket;
            io_buffer m_recv_buf;
            io_buffer m_send_buf;
        };


        template <class SocketType>
        inline void datagram_server<SocketType>::start(const typename protocol_type::endpoint bind_addr) {
            m_socket.open(bind_addr.protocol());
            error_code ignore_error;
            m_socket.set_option(typename socket_type::reuse_address(true), ignore_error);
            m_socket.bind(bind_addr);
            async_receive();
        }


        template <class SocketType>
        inline void datagram_server<SocketType>::async_receive() noexcept {
            typename protocol_type::endpoint remote_endpoint;
            m_socket.async_receive_from(asio::buffer(m_recv_buf.begin_write(), m_recv_buf.available()), remote_endpoint,
                [=] (const error_code error, size_t bytes_recvd) noexcept {
                    if (not error) {
                        m_recv_buf.complete_write(bytes_recvd);
                        try {
                            if (handle_data(m_recv_buf, m_send_buf) == SEND_REPLY_AND_READ) {
                                async_send_all(remote_endpoint);
                            }
                        } catch (const std::exception &) { /* swallow exception */ }
                    } else {
                        if (error == io_error::operation_aborted) {
                            // we were closed from the outside
                            return;
                        }
                    }
                    m_recv_buf.reset();
                    this->async_receive();
                });
        }


        template <class SocketType>
        inline void datagram_server<SocketType>::async_send_all(const typename protocol_type::endpoint to) noexcept {
            auto need_to_send = m_send_buf.non_read();
            m_socket.async_send_to(asio::buffer(m_send_buf.begin_read(), need_to_send), to,
                [=](error_code error, size_t bytes_sent) {
                    // TODO: How to handle errors
                    m_send_buf.complete_read(need_to_send);
                });
        }


    } // namespace net

} // namespace cachelot

/// @}

#endif // CACHELOT_NET_SOCKET_DATAGRAM_H_INCLUDED

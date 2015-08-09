#ifndef CACHELOT_NET_SOCKET_STREAM_H_INCLUDED
#define CACHELOT_NET_SOCKET_STREAM_H_INCLUDED

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

namespace cachelot {

    namespace net {

        /**
         * stream_connection is a network session for the `SOCK_STREAM` sockets
         *
         * @tparam SocketType is a socket (TCP / unix) provided by boost::asio
         * @tparam Conversation is an application-level protocol implementation over stream connection
         * TODO: Conversation description
         */
        template <class SocketType, class Conversation>
        class stream_connection {
            typedef stream_connection<SocketType, Conversation> this_type;
            stream_connection(const stream_connection &) = delete;
            stream_connection & operator= (const stream_connection &) = delete;
        protected:
            /// constructor
            explicit stream_connection(io_service & io_svc, const size_t rcvbuf_max = default_max_buffer_size, const size_t sndbuf_max = default_max_buffer_size);

            /// virtual destructor
            virtual ~stream_connection() = default;

            /// React on the data
            /// Parse incoming message from the `recv_buf`, call the Cache API functions and write reply into the `send_buf`
            /// @return ConversationReply indicates whether to send reply or just wait for more data
            virtual net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept = 0;

        public:
            /// Type of the underlying socket
            typedef SocketType socket_type;
            /// Transport layer protocol
            typedef typename SocketType::protocol_type protocol_type;

            /// underlying socket
            SocketType & socket() noexcept { return m_socket; }

            /// check whether connection is open
            bool is_open() const noexcept { return m_socket.is_open(); }

            /// start communication for connection
            void start() noexcept { async_receive_some(); }

            /// immediately cancel all pending operations and close the connection
            void close() noexcept;

        private:

            /// start asynchronous receive operation, call the Conversation::handle_data on complete
            void async_receive_some() noexcept;

            /// start asynchronous send of the send buffer
            void async_send_all() noexcept;

            /// schedule arbitrary function into IO loop
            template <typename Function>
            void post(Function fun) noexcept { m_socket.get_io_service().post(fun); }

            /// publish dynamic stats
            void publish_stats() noexcept;

            /// schedule deletion of this connection after the last operation completion
            void suicide() noexcept;

        private:
            SocketType m_socket;
            io_buffer m_recv_buf;
            io_buffer m_send_buf;
            bool m_killed;
        };


        /**
         * stream_server is an acceptor and connection manager for the `SOCK_STREAM` sockets (TCP/IP and local unix stream socket)
         *
         * @tparam SocketType -     Socket implementation from the boost::asio
         * @tparam ImplType -       Actual protocol_type Server implementation class, must be derived from stream_server
         *                          stream_server expects that ImplType class provides the following functions:
         *                          `ConversationType * new_conversation()` to create new conversations
         *                          `delete_conversation(ConversationType *)` to delete broken / expired conversations
         */
        template <class SocketType, class ImplType>
        class stream_server {
            typedef stream_server<SocketType, ImplType> this_type;
            typedef SocketType socket_type;
            typedef typename socket_type::protocol_type protocol_type;
        public:
            /// constructor
            explicit stream_server(io_service & ios)
                : m_ios(ios)
                , m_acceptor(ios) {
                static_assert(std::is_base_of<this_type, ImplType>::value, "<ImplType> must be derived from the stream_server");
            }

            stream_server(const stream_server &) = delete;
            stream_server & operator= (const stream_server &) = delete;

            /// start accept connections
            void start(const typename protocol_type::endpoint bind_addr);

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


///////////// stream connection implementation ////////////////////


        template <class Sock, class Conversation>
        inline stream_connection<Sock, Conversation>::stream_connection(io_service & io_svc, const size_t rcvbuf_max, const size_t sndbuf_max)
            : m_socket(io_svc)
            , m_recv_buf(default_min_buffer_size, rcvbuf_max)
            , m_send_buf(default_min_buffer_size, sndbuf_max)
            , m_killed(false) {
            static_assert(std::is_base_of<stream_connection<Sock, Conversation>, Conversation>::value, "Conversation must be derived class");
        }


        template <class Sock, class Conversation>
        inline void stream_connection<Sock, Conversation>::async_receive_some() noexcept {
            if (m_killed) { return; }
            m_socket.async_read_some(asio::buffer(m_recv_buf.begin_write(), m_recv_buf.available()),
                [=](const error_code error, const size_t bytes_received) {
                    bytes receive_result;
                    if (not error) {
                        m_recv_buf.confirm_write(bytes_received);
                        ConversationReply reply = handle_data(m_recv_buf, m_send_buf);
                        m_recv_buf.compact();
                        switch (reply) {
                        case SEND_REPLY_AND_READ:
                            async_send_all();
                            // there is no `break` so we'll continue receive
                        case READ_MORE:
                            async_receive_some();
                            break;
                        case CLOSE_IMMEDIATELY:
                            suicide();
                            break;
                        }
                    } else {
                        if (error == io_error::message_size) {
                            m_recv_buf.confirm_write(bytes_received);
                            async_receive_some();
                        } else {
                            suicide();
                        }
                    }
                });
        }


        template <class Sock, class Conversation>
        inline void stream_connection<Sock, Conversation>::async_send_all() noexcept {
            if (m_killed) { return; }
            asio::async_write(m_socket, asio::buffer(m_send_buf.begin_read(), m_send_buf.non_read()), asio::transfer_all(),
                [=](error_code error, size_t bytes_sent) {
                    if (not error) {
                        debug_assert(m_send_buf.non_read() == bytes_sent);
                        m_send_buf.confirm_read(bytes_sent);
                        m_send_buf.compact();
                    } else {
                        suicide();
                    }
                });
        }
        

        template <class Sock, class Conversation>
        inline void stream_connection<Sock, Conversation>::close() noexcept {
            if (is_open()) {
                error_code ignored;
                // try to shutdown the connection gracefully
                m_socket.shutdown(Sock::shutdown_both, ignored);
                m_socket.close(ignored);
            }
        }


        template <class Sock, class Conversation>
        inline void stream_connection<Sock, Conversation>::suicide() noexcept {
            if (not m_killed) {
                m_killed = true;
                close();
                post([=]() { delete this; } );
            }
        }


///////////// stream server implementation ///////////////////////


        template <class SocketType, class ImplType>
        inline void stream_server<SocketType, ImplType>::start(const typename protocol_type::endpoint bind_addr) {
            m_acceptor.open(bind_addr.protocol());
            error_code ignore_error;
            m_acceptor.set_option(typename protocol_type::acceptor::reuse_address(true), ignore_error);
            m_acceptor.bind(bind_addr);
            m_acceptor.listen();
            async_accept();
        }
        

        template <class SocketType, class ImplType>
        inline void stream_server<SocketType, ImplType>::async_accept() {
            try {
                auto new_conversation = static_cast<ImplType *>(this)->new_conversation();
                m_acceptor.async_accept(new_conversation->socket(),
                    [=](const error_code error) {
                        if (not error) {
                            new_conversation->start();
                        } else {
                            static_cast<ImplType *>(this)->delete_conversation(new_conversation);
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

#endif // CACHELOT_NET_SOCKET_STREAM_H_INCLUDED


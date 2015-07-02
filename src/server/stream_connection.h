#ifndef CACHELOT_NET_STREAM_CONNECTION_H_INCLUDED
#define CACHELOT_NET_STREAM_CONNECTION_H_INCLUDED

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
    protected:
        /// constructor
        explicit stream_connection(io_service & io_svc, const size_t rcvbuf_max = default_max_buffer_size, const size_t sndbuf_max = default_max_buffer_size);

        /// destructor
        ~stream_connection() {}

    public:
        typedef SocketType socket_type;
        typedef typename SocketType::protocol_type protocol_type;

        stream_connection(const stream_connection &) = delete;
        stream_connection & operator= (const stream_connection &) = delete;

        /// underlying socket
        SocketType & socket() noexcept { return m_socket; }

        /// check whether connection is open
        bool is_open() const noexcept { return m_socket.is_open(); }

        /// start communication for connection
        void start() noexcept;

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
        bytes receive_buffer() const noexcept;

    private:
        SocketType m_socket;
        io_buffer m_recv_buf;
        io_buffer m_send_buf;
        bool m_killed;
    };


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
        m_socket.async_receive_some(asio::buffer(m_recv_buf.begin_write(), m_recv_buf.available()),
            [=](const error_code error, const size_t bytes_received) {
                bytes receive_result;
                if (not error) {
                    m_recv_buf.complete_write(bytes_received);
                    ConversationReply reply = static_cast<Conversation *>(this)->handle_data(m_recv_buf, m_send_buf);
                    switch (reply) {
                    case SEND_REPLY:
                        async_send_all();
                        // there is no break so we'll continue receive
                    case READ_MORE:
                        async_receive_some();
                    }
                } else {
                    if (error == io_error::message_size) {
                        m_recv_buf.complete_write(bytes_received);
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
                    m_send_buf.complete_read(bytes_sent);
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

}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NET_STREAM_CONNECTION_H_INCLUDED

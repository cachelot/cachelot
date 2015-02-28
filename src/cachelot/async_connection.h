#ifndef CACHELOT_NET_ASYNC_CONNECTION_H_INCLUDED
#define CACHELOT_NET_ASYNC_CONNECTION_H_INCLUDED

#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <cachelot/network.h>
#endif

/**
 * @ingroup net
 * @{
 */

namespace cachelot {

    namespace net {

    /**
     * async_connection is a network session with asynchronos IO support
     *
     * @tparam SocketType is a socket (TCP / UDP / Unix / etc) provided by boost::asio
     * @tparam ConnectionType is a specific connection implementation based on async_connection
     */
    template <class SocketType, class ConnectionType>
    class async_connection {
        typedef async_connection<SocketType, ConnectionType> this_type;
    protected:
        /// constructor
        explicit async_connection(io_service & io_svc, const size_t rcvbuf_max = default_max_buffer_size, const size_t sndbuf_max = default_max_buffer_size);

        /// destructor
        ~async_connection() {}

    public:
        async_connection(const async_connection &) = delete;
        async_connection & operator= (const async_connection &) = delete;

        /// underlying socket
        SocketType & socket() noexcept { return m_socket; }

        /// checks whether connection is open
        bool is_open() const noexcept { return m_socket.is_open(); }

        /// receive until `terminator` will be found
        ///
        /// @param terminator - terminator byte sequence, receiving ends when it will be found
        /// @param on_complete - completion handler `void on_complete(bytes data)` will be called when either terminator will be called or error will happen
        template <typename Callback>
        void async_receive_until(const bytes terminator, Callback on_complete) noexcept;

        /// read N bytes
        ///
        /// @param n - number of bytes to read
        /// @param on_complete - completion handler `void on_complete(bytes data)` will be called when either terminator will be called or error will happen
        template <typename Callback>
        void async_receive_n(const size_t n, Callback on_complete) noexcept;


        /// send whole buffer sequence
        template <typename Callback>
        void async_send_all(Callback on_complete) noexcept;

        /// send buffer to write into
        io_buffer & send_buffer() noexcept { return m_snd_buf; }

        /// immediately cancel all pending operations and close the connection
        void close() noexcept;

        /// schedule function into IO loop. In multithreaded environment all
        /// function calls of the single connection will be executed one by the same thread
        template <typename Function>
        void post(Function fun) noexcept { m_ios.post(fun); }

    private:
        bytes receive_buffer() const noexcept;

    private:
        io_service & m_ios;
        SocketType m_socket;
        io_buffer m_rcv_buf;
        io_buffer m_snd_buf;
    };

    template <class Sock, class Conn>
    inline async_connection<Sock, Conn>::async_connection(io_service & io_svc, const size_t rcvbuf_max, const size_t sndbuf_max)
        : m_ios(io_svc)
        , m_socket(io_svc)
        , m_rcv_buf(default_min_buffer_size, rcvbuf_max)
        , m_snd_buf(default_min_buffer_size, sndbuf_max) {
    }


    template <class Sock, class Conn> template <typename Callback>
    inline void async_connection<Sock, Conn>::async_receive_until(const bytes terminator, Callback on_complete) noexcept {
        m_socket.async_read_some(asio::buffer(m_rcv_buf.begin_write(), m_rcv_buf.available()),
                [=](const error_code error, const size_t bytes_received) {
                    bytes receive_result;
                    if (not error) {
                        m_rcv_buf.confirm(bytes_received);
                        net_debug_mtrace("async_receive_until -> received %u bytes (buffer: %u bytes)", bytes_received, m_rcv_buf.size());
                        receive_result = m_rcv_buf.try_read_until(terminator);
                        if (receive_result) {
                            on_complete(error, receive_result);
                        } else {
                            // read more data
                            post([=]() { this->async_receive_until(terminator, on_complete); });
                        }
                    } else {
                        on_complete(error, bytes()); // completed with error
                    }
                });
    }

    template <class Sock, class Conn> template <typename Callback>
    inline void async_connection<Sock, Conn>::async_receive_n(const size_t n, Callback on_complete) noexcept {
        debug_assert(n > 0);
        if (m_rcv_buf.unread() >= n) {
            // we already have requested data in buffer after previous read
            on_complete(error::success, m_rcv_buf.read(n));
        } else {
            size_t bytes_to_receive = n - m_rcv_buf.unread();
            asio::async_read(m_socket, asio::buffer(m_rcv_buf.begin_write(bytes_to_receive), bytes_to_receive), transfer_exactly(bytes_to_receive),
                [=](const error_code error, const size_t bytes_received) {
                    bytes receive_result;
                    if (!error) {
                        debug_assert(bytes_received == bytes_to_receive);
                        m_rcv_buf.confirm(bytes_received);
                        net_debug_mtrace("async_receive_n -> received %u bytes (buffer: %u bytes)", bytes_received, bytes_received, m_rcv_buf.size());
                        receive_result = m_rcv_buf.read(n);
                    }
                    on_complete(error, receive_result);
                });
        }
    }

    template <class Sock, class Conn> template <typename Callback>
    inline void async_connection<Sock, Conn>::async_send_all(Callback on_complete) noexcept {
        // start asynchronous sending
        asio::async_write(m_socket, asio::buffer(m_snd_buf.begin_read(), m_snd_buf.unread()), transfer_all(),
            [=](error_code error, size_t bytes_sent) {
                debug_assert(bytes_sent == m_snd_buf.unread());
                m_snd_buf.read(bytes_sent);
                on_complete(error);
            });
    }

    template <class Sock, class Conn>
    inline void async_connection<Sock, Conn>::close() noexcept {
        if (m_socket.is_open()) {
            error_code __;
            // try to shutdown the connection gracefully
            m_socket.shutdown(Sock::shutdown_both, __);
            m_socket.close(__);
        }
    }

}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NET_ASYNC_CONNECTION_H_INCLUDED

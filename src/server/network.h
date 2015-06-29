#ifndef CACHELOT_NETWORK_H_INCLUDED
#define CACHELOT_NETWORK_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif
#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#  include <server/io_buffer.h>
#endif

// Disable warnings in boost::asio
#if defined __GNUC__
#  pragma GCC system_header
#elif defined _MSC_VER
#  pragma warning(push, 1)
#endif
#ifndef BOOST_ASIO_HPP
// TODO: Build system options
#define BOOST_ASIO_DISABLE_THREADS
//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#  include <boost/asio.hpp>
#endif
#if defined _MSC_VER
#  pragma warning(pop)
#endif

/**
 * @defgroup net Networking and Connectivity
 * TCP, UDP, Unix Sockets asynchronous IO based on Boost.Asio
 * @{
 */

namespace cachelot {

    /// @ref net
    namespace net {

        namespace asio = boost::asio;
        namespace ip = boost::asio::ip;
        namespace local = boost::asio::local;

        using asio::io_service;
        using ip::tcp;
        using ip::udp;

        //typedef asio::basic_waitable_timer<chrono::steady_clock> waitable_timer;

        namespace placeholders = asio::placeholders;
        namespace io_error = asio::error;

        // completition conditions
        using asio::transfer_all;
        using asio::transfer_at_least;
        using asio::transfer_exactly;


        /// socket read / write operations same for both: stream and datagram sockets
        template <typename Protocol>
        struct socket_ops {
            typedef typename Protocol::endpoint endpoint_type;

            /// stream socket receive operation
            template <class MutableBufferSequence, typename Callback>
            static void async_receive(asio::basic_stream_socket<Protocol> & socket, MutableBufferSequence buffers, Callback on_complete) noexcept {
                const auto remote_endpoint = socket.remote_endpoint();
                socket.async_read_some(buffers,
                    [=](const error_code error, const size_t bytes_received) {
                        on_complete(error, bytes_received, remote_endpoint);
                    });
            }

            /// datagram socket receive operation
            template <class MutableBufferSequence, typename Callback>
            static void async_receive(asio::basic_datagram_socket<Protocol> & socket, MutableBufferSequence buffers, Callback on_complete) noexcept {
                endpoint_type endpoint;
                socket.async_receive_from(buffers, endpoint,
                    [=](const error_code error, const size_t bytes_received) {
                        on_complete(error, bytes_received, endpoint);
                    });
            }

            /// stream socket send operation
            template <class ConstBufferSequence, typename Callback>
            static void async_send(asio::basic_stream_socket<Protocol> & socket, const endpoint_type endpoint, ConstBufferSequence buffers, Callback on_complete) noexcept {
                (void)endpoint; // ignore sendto endpoint in the stream socket
                socket.async_write_some(buffers, on_complete);
            }

            /// datagram socket send operation
            template <class ConstBufferSequence, typename Callback>
            static void async_send(asio::basic_datagram_socket<Protocol> & socket, const endpoint_type endpoint, ConstBufferSequence buffers, Callback on_complete) noexcept {
                socket.async_send_to(buffers, endpoint, on_complete);
            }
        };


}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NETWORK_H_INCLUDED

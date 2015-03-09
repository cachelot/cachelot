#ifndef CACHELOT_NETWORK_H_INCLUDED
#define CACHELOT_NETWORK_H_INCLUDED

#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif
#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#  include <cachelot/io_buffer.h>
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
 * TCP/IP, UDP, Unix Sockets asynchronous IO based on Boost.Asio
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

    // completition conditions
    using asio::transfer_all;
    using asio::transfer_at_least;
    using asio::transfer_exactly;

}} // namespace cachelot::net

/// @}

#endif // CACHELOT_NETWORK_H_INCLUDED

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

        // shortcuts
        namespace asio = boost::asio;
        namespace ip = boost::asio::ip;
        using ip::tcp;
        using ip::udp;
        namespace local = boost::asio::local;
        using asio::io_service;

        namespace io_error = asio::error;

        enum ConversationReply {
            READ_MORE,
            SEND_REPLY_AND_READ,
            CLOSE_IMMEDIATELY
        };

        /// Unified TCP/UDP/Local converation interface
        class basic_conversation {
        protected:
            /// main interface function of the conversation called upon async receive completion
            /// Implementation shall read data from the `recv_buf` and write reply to the `send_buf`
            /// @return one of the possible replies to instruct what to do with the connection next
            virtual ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept = 0;

        public:
            basic_conversation() = default;
            virtual ~basic_conversation() {}
        };

    } // namespace net

} // namespace cachelot::net

/// @}

#endif // CACHELOT_NETWORK_H_INCLUDED

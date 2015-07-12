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
         * 
         * @tparam SocketType is a datagram socket from the boost::asio library
         * @tparam ConversationType is an implementation of the protocol handler
         */
        template <class SocketType, class ConversationType>
        class datagram_server {
            typedef datagram_server<SocketType, ConversationType> this_type;
            typedef SocketType socket_type;
            typedef typename socket_type::protocol_type protocol_type;
        public:
            /// constructor
            explicit datagram_server(io_service & ios)
                : m_ios(ios)
                , m_socket(ios)
                , m_recv_buf(default_min_buffer_size, default_max_buffer_size) {
            }

            /// virtual destructor
            virtual ~datagram_server() = default;

            datagram_server(const datagram_server &) = delete;
            datagram_server & operator= (const datagram_server &) = delete;

            /// start accept connections
            void start(const typename protocol_type::endpoint bind_addr);

            /// interrupt all activity
            void stop() noexcept {
                error_code ignore_error;
                m_socket.close(ignore_error);
            }

            io_service & get_io_service() noexcept { return m_ios; }

        protected:
            ///  create new conversation, actual implementation must override this
            virtual ConversationType * new_conversation() = 0;

            ///  delete the conversation, actual implementation must override this
            virtual void delete_conversation(ConversationType *) = 0;

        private:
            void async_receive() noexcept;

            void handle_data(bytes data, const typename protocol_type::endpoint remote_addr) noexcept;

            static uint32 hash_endpoint(const typename protocol_type::endpoint the_endpoint) noexcept;

        private:
            io_service & m_ios;
            socket_type m_socket;
            io_buffer m_recv_buf;
            dict<typename protocol_type::endpoint, std::unique_ptr<ConversationType>> m_conn_by_endpoint;
        };


        template <class SocketType, class ConversationType>
        inline void datagram_server<SocketType, ConversationType>::start(const typename protocol_type::endpoint bind_addr) {
            m_socket.open(bind_addr.protocol());
            error_code ignore_error;
            m_socket.set_option(typename socket_type::reuse_address(true), ignore_error);
            m_socket.bind(bind_addr);
            async_receive();
        }


        template <class SocketType, class ConversationType>
        inline void datagram_server<SocketType, ConversationType>::async_receive() noexcept {
            typename protocol_type::endpoint remote_endpoint;
            m_socket.async_receive_from(asio::buffer(m_recv_buf.begin_write(), m_recv_buf.available()), remote_endpoint,
                 [=] (const error_code error, size_t bytes_recvd) noexcept {
                     if (not error) {
                         m_recv_buf.complete_write(bytes_recvd);
                         try {
                             handle_data(m_recv_buf.read_all(), remote_endpoint);
                         } catch (const std::exception &) { /* swallow exception */ }
                     } else {
                         m_recv_buf.reset();
                     }
                     this->async_receive();
                 });
        }


        template <class SocketType, class ConversationType>
        inline void datagram_server<SocketType, ConversationType>::handle_data(bytes data, const typename protocol_type::endpoint remote_addr) noexcept {
            bool found; typename decltype(m_conn_by_endpoint)::iterator pos;
            tie(found, pos) = m_conn_by_endpoint.entry_for(remote_addr, hash_endpoint(remote_addr));
            if (found) {
                pos.value()->handle_data(data);
            } else {
                auto conversation = new_conversation();
                auto & socket = conversation->socket();
                socket.open(m_socket.protocol_type());
            }
        }


        template <class SocketType, class ConversationType>
        inline uint32 datagram_server<SocketType, ConversationType>::hash_endpoint(const typename protocol_type::endpoint the_endpoint) noexcept {
            constexpr auto hasher = fnv1a<uint32>::hasher();
            const auto addr_bytes = the_endpoint.address().to_bytes();
            uint32 checksum = hasher(bytes(&addr_bytes.front(), addr_bytes.size()));
            checksum = (checksum ^ the_endpoint.port());
            return checksum;
        }

    } // namespace net

} // namespace cachelot

/// @}

#endif // CACHELOT_NET_DATAGRAM_SERVER_H_INCLUDED

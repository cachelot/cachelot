#ifndef CACHELOT_CONVERSATION_H_INCLUDED
#define CACHELOT_CONVERSATION_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup memcached
/// @{

#include <server/memcached/memcached.h>
#include <server/stream_connection.h>
#include <server/stream_server.h>
#include <server/datagram_server.h>

namespace cachelot {

    namespace memcached {

        /// Memcached stream socket protocol conversation
        template <class SocketType>
        class StreamSocketConversation : public net::stream_connection<SocketType, StreamSocketConversation<SocketType>> {
            typedef net::stream_connection<SocketType, StreamSocketConversation<SocketType>> super;
        public:
            /// constructor
            explicit StreamSocketConversation(cache::Cache & the_cache, net::io_service & io_svc, const size_t rcvbuf_max, const size_t sndbuf_max)
                : super(io_svc, rcvbuf_max, sndbuf_max)
                , cache_api(the_cache) {
            }


        protected:
            /// @copydoc stream_connection::handle_data()
            net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override {
                try {
                    return handle_received_data(recv_buf, send_buf, cache_api);
                } catch (const std::exception &) {
                    return net::CLOSE_IMMEDIATELY;
                }
            }
        private:
            cache::Cache & cache_api;
        };


        /// Implementation of the memcached stream server
        template <class StreamSocketType>
        class StreamServer : public net::stream_server<StreamSocketType, StreamServer<StreamSocketType>> {
            typedef net::stream_server<StreamSocketType, StreamServer<StreamSocketType>> super;
            typedef StreamSocketConversation<StreamSocketType> ConversationType;
        public:
            explicit StreamServer(cache::Cache & the_cache, net::io_service & io_svc)
                : super(io_svc)
                , cache_api(the_cache) {
            }

            ConversationType * new_conversation() { return new ConversationType(cache_api, super::get_io_service(), settings.net.max_rcv_buffer_size, settings.net.max_snd_buffer_size); }

            void delete_conversation(ConversationType * c) { delete c; }

        private:
            cache::Cache & cache_api;
        };


        class DatagramConversation {
        public:
            DatagramConversation(cache::Cache & the_cache, net::io_service & io_svc);
        private:
            cache::Cache & cache_api;
            /// UDP Protocol header
            struct udp_frame_header {
                static constexpr size_t size = 8;
                uint16 request_id;
                uint16 sequence_no;
                uint16 packets_in_msg;
                uint16 reserved;

                static udp_frame_header fromBytes(bytes raw);
            };
        };

        inline DatagramConversation::udp_frame_header DatagramConversation::udp_frame_header::fromBytes(bytes raw) {
            debug_assert(unaligned_bytes(raw.begin(), alignof(uint16)) == 0);
            if (raw.length() < udp_frame_header::size) {
                throw system_error(error::make_error_code(error::udp_header_size));
            }
            uint16 request_id = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 0));
            uint16 sequence_no = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 1));
            uint16 packets_in_msg = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 2));
            uint16 reserved = *reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 3);
            if (reserved != 0) {
                throw system_error(error::make_error_code(error::udp_proto_reserverd));
            }
            return udp_frame_header { request_id, sequence_no, packets_in_msg, reserved };
        }


        class DatagramServer : public net::datagram_server<net::udp::socket, DatagramConversation> {
            typedef net::datagram_server<net::udp::socket, DatagramConversation> super;
        public:
            explicit DatagramServer(cache::Cache & the_cache, net::io_service & io_svc)
                : super(io_svc)
                , cache_api(the_cache) {
            }

        protected:

            DatagramConversation * new_conversation() override;

            void delete_conversation(DatagramConversation *)  override;


        private:
            cache::Cache & cache_api;
        };





        typedef StreamServer<net::tcp::socket> tcp_server;
        typedef StreamServer<net::local::stream_protocol::socket> unix_socket_server;


    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED

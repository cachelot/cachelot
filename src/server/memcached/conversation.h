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
#include <server/socket_stream.h>
#include <server/socket_datagram.h>

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


        class DatagramServer : public net::datagram_server<net::udp::socket> {
            typedef net::datagram_server<net::udp::socket> super;
        public:
            explicit DatagramServer(cache::Cache & the_cache, net::io_service & io_svc)
                : super(io_svc)
                , cache_api(the_cache) {
            }

        protected:
            net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override {
                auto w_savepoint = send_buf.write_savepoint();
                try {
                    handle_udp_frame_header(recv_buf, send_buf);
                    return handle_received_data(recv_buf, send_buf, cache_api);
                } catch (const std::exception & exc) {
                    send_buf.discard_written(w_savepoint);
                    return net::CLOSE_IMMEDIATELY;
                }
            }

            void handle_udp_frame_header(io_buffer & recv_buf, io_buffer & send_buf) {
                static constexpr size_t udp_frame_header_size = 8;
                // UDP frame header is:
                // 0               16               32               48               64
                // +----------------+----------------+----------------+----------------+
                // |  request id    |   sequence_no  | packets_in_msg |    reserved    |
                // +----------------+----------------+----------------+----------------+
                // All number are unsigned 16 bit integers in network byte order

                if (recv_buf.non_read() < udp_frame_header_size) {
                    throw system_error(error::udp_header_size);
                }
                bytes raw = bytes(recv_buf.begin_read(), udp_frame_header_size);
                recv_buf.complete_read(udp_frame_header_size);

                uint16 sequence_no = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 1));
                uint16 packets_in_msg = ntohs(*reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 2));
                if (sequence_no != 0 || packets_in_msg > 1) {
                    // multi-frame requests are not supported
                    throw system_error(error::not_implemented);
                }
                uint16 reserved = *reinterpret_cast<const uint16 *>(raw.begin() + sizeof(uint16) * 3);
                if (reserved != 0) {
                    throw system_error(error::udp_proto_reserverd);
                }
                // write UDP header back to the response
                auto dest = send_buf.begin_write(raw.length());
                std::memcpy(dest, raw.begin(), raw.length());
                send_buf.complete_write(raw.length());
            }

        private:
            cache::Cache & cache_api;
        };


        typedef StreamServer<net::tcp::socket> tcp_server;
        typedef StreamServer<net::local::stream_protocol::socket> unix_socket_server;


    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED

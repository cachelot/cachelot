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
                , cache_api(the_cache)
                , m_protocol_protocol_handler() {
            }


        protected:
            /// @copydoc stream_connection::handle_data()
            net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override {
                try {
                    if (not m_protocol_protocol_handler) {
                        m_protocol_protocol_handler = basic_protocol_handler::determine_protocol(recv_buf, cache_api);
                    }
                    return m_protocol_protocol_handler->handle_data(recv_buf, send_buf);
                } catch (const std::exception &) {
                    return net::CLOSE_IMMEDIATELY;
                }
            }
        private:
            cache::Cache & cache_api;
            std::unique_ptr<basic_protocol_handler> m_protocol_protocol_handler;
        };


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

        typedef StreamServer<net::tcp::socket> tcp_server;

        typedef StreamServer<net::local::stream_protocol::socket> unix_socket_server;


    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED

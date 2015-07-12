#ifndef CACHELOT_NET_STREAM_SERVER_H_INCLUDED
#define CACHELOT_NET_STREAM_SERVER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_NETWORK_H_INCLUDED
#  include <server/network.h>
#endif
#ifndef CACHELOT_NET_STREAM_CONNECTION_H_INCLUDED
#  include <server/stream_connection.h>
#endif


/// @ingroup net
/// @{

namespace cachelot {

    namespace net {

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

#endif // CACHELOT_NET_STREAM_SERVER_H_INCLUDED

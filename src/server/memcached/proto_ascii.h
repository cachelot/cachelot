#ifndef CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED
#define CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <server/memcached/memcached.h>

/// @ingroup memcached 
/// @{

namespace cachelot {

    namespace memcached { namespace ascii {

        class protocol_handler : public basic_protocol_handler {
        public:
            /// constructor
            explicit protocol_handler(cache::Cache & the_cache) : basic_protocol_handler(the_cache) {}

            /// @copydoc basic_protocol_handler::handle_data()
            virtual net::ConversationReply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override;

        private:
            /// Parse the name of the `command`
            cache::Command parse_command_name(bytes command) noexcept;

            /// Read key value from the arguments sequence
            /// @return the key and rest of the args
            tuple<bytes, bytes> parse_key(bytes args);

            /// parse optional `noreply` clause
            bool parse_noreply(const bytes buffer);

            /// Handle on of the: `get` `gets` commands
            net::ConversationReply handle_retrieval_command(cache::Command cmd, bytes args, io_buffer & send_buf);

            /// Handle on of the: `add`, `set`, `replace`, `cas`, `append`, `prepend` commands
            net::ConversationReply handle_storage_command(cache::Command cmd, bytes args, io_buffer & recv_buf, io_buffer & send_buf);

            /// Handle the `delete` command
            net::ConversationReply handle_delete_command(cache::Command cmd, bytes args, io_buffer & send_buf);

            /// Handle on of the: `incr` `decr` commands
            net::ConversationReply handle_arithmetic_command(cache::Command cmd, bytes args, io_buffer & send_buf);

            /// Handle the `touch` command
            net::ConversationReply handle_touch_command(cache::Command cmd, bytes args, io_buffer & send_buf);

            /// Handle the `stats` command
            net::ConversationReply handle_statistics_command(cache::Command cmd, bytes args, io_buffer & send_buf);

            /// Write one of the cache responses if `noreply` is not specified, none otherwise
            net::ConversationReply reply_with_response(io_buffer & send_buf, cache::Response response, bool noreply);
        };

    }} // namespace memcached::ascii

} // namespace cachelot

/// @}

#endif // CACHELOT_MEMCACHED_PROTO_ASCII_H_INCLUDED

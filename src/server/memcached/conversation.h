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

namespace cachelot {

    namespace memcached {

        enum ConversationReply {
            READ_MORE,
            REPLY,
            REPLY_AND_CLOSE,
            CLOSE_IMMEDIATELY
        };

        /// Memcached protocol conversation
        class Conversation {
            enum {
                DETERMINE_PROTOCOL,
                EXPECT_COMMAND
            } parse_state;
        public:
            /// constructor
            Conversation(cache::Cache & the_cache);

            /// react on the new data
            ConversationReply handle_data(bytes data) noexcept;

        private:
            cache::Cache & cache;
            io_buffer m_incoming;
            io_buffer m_outgoing;
        };


    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED

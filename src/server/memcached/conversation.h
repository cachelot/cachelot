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

        /// Memcached protocol conversation
        class Conversation : public net::basic_conversation {
        public:
            /// constructor
            Conversation(cache::Cache & the_cache);

            /// react on the new data
            Reply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override;

        private:
            cache::Cache & cache;
        };


    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED

#include <cachelot/common.h>
#include <server/memcached/proto_binary.h>
#include <thirdparty/memcached/protocol_binary.h>

namespace cachelot {

    namespace memcached { namespace binary {

        const uint8 MAGIC = PROTOCOL_BINARY_REQ;


        /// Main function that process binary protocol packets
        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api) {
            throw system_error(cachelot::error::not_implemented);
        }

    }} // namespace memcached::binary

} // namespace cachelot


#include <cachelot/common.h>
#include <server/memcached/memcached.h>
#include <server/memcached/proto_ascii.h>
#include <server/memcached/proto_binary.h>

namespace cachelot {

    namespace memcached {

        net::ConversationReply handle_received_data(io_buffer & recv_buf, io_buffer & send_buf, cache::Cache & cache_api) {
            if (recv_buf.non_read() > 0) {
                if (static_cast<decltype(binary::MAGIC)>(*recv_buf.begin_read()) == binary::MAGIC) {
                    return binary::handle_received_data(recv_buf, send_buf, cache_api);
                } else {
                    return ascii::handle_received_data(recv_buf, send_buf, cache_api);
                }
            } else {
                return net::ConversationReply::READ_MORE;
            }
        }

    } // namespace memcached

} // namespace cachelot


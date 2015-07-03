#include <cachelot/common.h>
#include <server/memcached/memcached.h>
#include <server/memcached/proto_ascii.h>
#include <server/memcached/proto_binary.h>

namespace cachelot {

    namespace memcached {

        std::unique_ptr<basic_protocol_handler> basic_protocol_handler::determine_protocol(io_buffer & recv_buf, cache::Cache & the_cache) {
            if (recv_buf.non_read() > 0) {
                if (static_cast<decltype(binary::MAGIC)>(*recv_buf.begin_read()) == binary::MAGIC) {
                    recv_buf.complete_read(1); // read magic byte
                    throw system_error(cachelot::error::not_implemented);
                } else {
                    return std::unique_ptr<basic_protocol_handler>(new ascii::protocol_handler(the_cache));
                }
            } else {
                throw system_error(error::incomplete_request);
            }
        }

    } // namespace memcached

} // namespace cachelot


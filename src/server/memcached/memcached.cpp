#include <cachelot/common.h>
#include <server/memcached/memcached.h>
#include <server/memcached/proto_ascii.h>
#include <server/memcached/proto_binary.h>

namespace cachelot {

    namespace memcached {

        std::unique_ptr<basic_protocol_parser> basic_protocol_parser::determine_protocol(io_buffer & recv_buf) {
            if (recv_buf.non_read() > 0) {
                if (static_cast<decltype(binary::MAGIC)>(*recv_buf.begin_read()) == binary::MAGIC) {
                    recv_buf.complete_read(1); // read magic byte
                    throw system_error(cachelot::error::not_implemented);
                } else {
                    return std::unique_ptr<basic_protocol_parser>(new ascii::protocol_parser());
                }
            } else {
                throw system_error(error::not_enough_data);
            }
        }

    } // namespace memcached

} // namespace cachelot


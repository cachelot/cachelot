#include <cachelot/common.h>
#include <server/memcached/memcached.h>
#include <server/memcached/proto_ascii.h>
#include <server/memcached/proto_binary.h>

namespace cachelot {

    /// @ref memcached
    namespace memcached {

        basic_protocol_parser & basic_protocol_parser::determine_protocol(bytes data) {
            if (data.length() > 0) {
                if (static_cast<decltype(binary::MAGIC)>(*data.begin()) == binary::MAGIC) {
                    throw system_error(cachelot::error::not_implemented);
                } else {
                    static ascii::protocol_parser ascii_proto_parser;
                    return ascii_proto_parser;
                }
            } else {
                throw system_error(error::not_enough_data);
            }
        }


    } // namespace memcached

} // namespace cachelot

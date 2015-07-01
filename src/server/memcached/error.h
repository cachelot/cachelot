#ifndef CACHELOT_MEMCACHED_ERROR_H_INCLUDED
#define CACHELOT_MEMCACHED_ERROR_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif

/// @ingroup memcached
/// @{

namespace cachelot {

    #define MEMCACHED_PROTO_ERROR_ENUM(x)                                       \
        x(not_enough_data, "Expected more data in the message")                 \
        x(key_length, "Maximum key length exceeded")                            \
        x(value_length, "Maximum value length exceeded")                        \
        x(crlf_expected, "Invalid request: \\r\\n expected")                    \
        x(value_crlf_expected, "Invalid value: \\r\\n expected")                \
        x(key_expected, "Invalid request: key expected")                        \
        x(integer_conv, "Invalid request: failed to convert integer argument")  \
        x(integer_range, "Invalid request: integer value is out of range")      \
        x(noreply_expected, "Invalid request: expected noreply")                \
        x(udp_header_size, "UDP packet header is too small")                    \
        x(udp_proto_reserverd, "UDP reserved flag expected to be zero")

    /// @ref error
    namespace error {

        /// Memcached protocol-related errors
        enum protocol_error_code {
            #define MEMCACHED_PROTO_ERROR_ENUM_CODE(code, _) code,
            MEMCACHED_PROTO_ERROR_ENUM(MEMCACHED_PROTO_ERROR_ENUM_CODE)
            #undef MEMCACHED_PROTO_ERROR_ENUM_CODE
        };


        namespace internal {

            class memcached_protocol_error_category : public error_category {
            public:
                memcached_protocol_error_category() = default;

                virtual const char * name() const noexcept override { return "Memcached protocol error"; }

                virtual string message(int value) const override {
                    switch (static_cast<protocol_error_code>(value)) {
                        #define MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG(code, msg) \
                        case code: return msg;
                        MEMCACHED_PROTO_ERROR_ENUM(MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG)
                        #undef MEMCACHED_PROTO_ERROR_ENUM_CODE_MSG
                        default: return "Invalid error code";
                    }
                }
            };

        } // namespace internal


        inline const error_category & get_protocol_error_category() noexcept {
            static internal::memcached_protocol_error_category category_instance;
            return category_instance;
        }


        inline error_code make_error_code(protocol_error_code ec) noexcept {
            return error_code(static_cast<int>(ec), get_protocol_error_category());
        }


    } // namespace error

    using error::get_protocol_error_category;

} // namespace cachelot

/// @}

namespace boost { namespace system {
    // register error_code type
    template <> struct is_error_code_enum<cachelot::error::protocol_error_code> {
        static const bool value = true;
    };
} } // namespace boost::system

#endif // CACHELOT_MEMCACHED_ERROR_H_INCLUDED

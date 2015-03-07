#ifndef CACHELOT_PROTO_MEMCACHED_H_INCLUDED
#define CACHELOT_PROTO_MEMCACHED_H_INCLUDED

#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_NET_ASYNC_CONNECTION_H_INCLUDED
#  include <cachelot/async_connection.h>
#endif
#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#  include <cachelot/io_buffer.h>
#endif
#ifndef CACHELOT_IO_SERIALIZATION_H_INCLUDED
#  include <cachelot/io_serialization.h>
#endif
#ifndef CACHELOT_THREAD_SAFE_CACHE_H_INCLUDED
#  include <cachelot/thread_safe_cache.h>
#endif
#ifndef CACHELOT_HASH_FNV1A_H_INCLUDED
#  include <cachelot/hash_fnv1a.h>
#endif
#ifndef CACHELOT_SETTINGS_H_INCLUDED
#  include <cachelot/settings.h>
#endif

/// @defgroup memcached Memcached protocols implementation
/// @{

namespace cachelot {

    namespace memcached {

#define MEMCACHED_RESPONSES_ENUM(x) \
        x(STORED)                           \
        x(NOT_STORED)                       \
        x(EXISTS)                           \
        x(NOT_FOUND)                        \
        x(DELETED)                          \
        x(TOUCHED)                          \
        x(NOT_A_RESPONSE)

        enum Response {
#define MEMCACHED_RESPONSES_ENUM_ELEMENT(resp) resp,
            MEMCACHED_RESPONSES_ENUM(MEMCACHED_RESPONSES_ENUM_ELEMENT)
#undef MEMCACHED_RESPONSES_ENUM_ELEMENT
        };

        inline const bytes AsciiResponse(Response r) noexcept {
#define MEMCACHED_RESPONSES_ENUM_STRELEMENT(resp) case resp: return bytes::from_literal(CACHELOT_PP_STR(resp));
            switch (r) {
                    MEMCACHED_RESPONSES_ENUM(MEMCACHED_RESPONSES_ENUM_STRELEMENT)
            };
            debug_assert(false && "something is terribly wrong");
            return bytes();
#undef MEMCACHED_RESPONSES_ENUM_STRELEMENT
        }

#define MEMCACHED_ERRTYPES_ENUM(x)  \
        x(ERROR) \
        x(CLIENT_ERROR) \
        x(SERVER_ERROR)

        enum ErrorType {
#define MEMCACHED_ERRTYPES_ENUM_ELEMENT(resp) resp,
            MEMCACHED_ERRTYPES_ENUM(MEMCACHED_ERRTYPES_ENUM_ELEMENT)
#undef MEMCACHED_ERRTYPES_ENUM_ELEMENT
        };

        inline const bytes AsciiErrorType(ErrorType et) noexcept {
#define MEMCACHED_ERRTYPES_ENUM_STRELEMENT(errty) case errty: return bytes::from_literal(CACHELOT_PP_STR(errty));
            switch (et) {
                    MEMCACHED_ERRTYPES_ENUM(MEMCACHED_ERRTYPES_ENUM_STRELEMENT)
            };
            debug_assert(false && "something went terribly wrong");
            return bytes();
#undef MEMCACHED_ERRTYPES_ENUM_STRELEMENT
        }



    /// exception class that indicates request execution error
    struct memcached_error : public std::runtime_error {
        explicit memcached_error(const char * msg) : std::runtime_error(msg) {}
        explicit memcached_error(const string & msg) : std::runtime_error(msg) {}
    };

    /// exception class that indicates that given comman is unknown
    struct non_existing_request_error : memcached_error {
        non_existing_request_error() : memcached_error("ERROR") {}
    };

    /// exception class that indicates error in client request
    struct client_error : public memcached_error {
        explicit client_error(const char * msg) : memcached_error(msg) {}
    };

    typedef fnv1a<cache::hash_type>::hasher HashFunction;


} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

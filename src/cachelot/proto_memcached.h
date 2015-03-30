#ifndef CACHELOT_PROTO_MEMCACHED_H_INCLUDED
#define CACHELOT_PROTO_MEMCACHED_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


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
#ifndef CACHELOT_CACHE_H_INCLUDED
#  include <cachelot/cache.h>
#endif
#ifndef CACHELOT_SETTINGS_H_INCLUDED
#  include <cachelot/settings.h>
#endif


/// @defgroup memcached Memcached protocols implementation
/// @{

namespace cachelot {

    /// @ref memcached
    namespace memcached {


#define CACHE_ERRTYPES_ENUM(x)  \
        x(ERROR)                \
        x(CLIENT_ERROR)         \
        x(SERVER_ERROR)

        enum ErrorType {
#define CACHE_ERRTYPES_ELEMENT(resp) resp,
            CACHE_ERRTYPES_ENUM(CACHE_ERRTYPES_ELEMENT)
#undef CACHE_ERRTYPES_ELEMENT
        };

#define CACHE_ERRTYPES_STRELEMENT(errty) bytes::from_literal(CACHELOT_PP_STR(errty)),
        constexpr bytes __AsciiErrorTypes[] {
            CACHE_ERRTYPES_ENUM(CACHE_ERRTYPES_STRELEMENT)
        };
#undef CACHE_ERRTYPES_STRELEMENT

        inline constexpr bytes AsciiErrorType(ErrorType et) noexcept {
            return __AsciiErrorTypes[static_cast<unsigned>(et)];
        }

        /// TODO: replace exception classes with error_code / system_error
        /// exception class that indicates request execution error
        struct memcached_error : public std::runtime_error {
            explicit memcached_error(const char * msg) : std::runtime_error(msg) {}
            explicit memcached_error(const string & msg) : std::runtime_error(msg) {}
        };

        /// exception class that indicates that given comman is unknown
        struct non_existing_command_error : memcached_error {
            non_existing_command_error() : memcached_error("ERROR") {}
        };

        /// exception class that indicates error in client request
        struct client_error : public memcached_error {
            explicit client_error(const char * msg) : memcached_error(msg) {}
        };


} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

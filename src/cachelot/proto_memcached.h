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
#ifndef CACHELOT_CACHE_H_INCLUDED
#  include <cachelot/cache.h>
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

// Primary table of all supported commands and their classes
#define MEMCACHED_COMMANDS_ENUM(x)              \
        x(ADD, STORAGE_COMMAND)             \
        x(APPEND, STORAGE_COMMAND)          \
        x(CAS, STORAGE_COMMAND)             \
        x(DECR, ARITHMETIC_COMMAND)         \
        x(DELETE, DELETE_COMMAND)           \
        x(GET, RETRIEVAL_COMMAND)           \
        x(GETS, RETRIEVAL_COMMAND)          \
        x(INCR, ARITHMETIC_COMMAND)         \
        x(PREPEND, STORAGE_COMMAND)         \
        x(REPLACE, STORAGE_COMMAND)         \
        x(SET, STORAGE_COMMAND)             \
        x(TOUCH, TOUCH_COMMAND)             \
        x(QUIT, QUIT_COMMAND)               \
        x(SYNC, SYNC_COMMAND)               \
        x(UNDEFINED, UNDEFINED_COMMAND)


        enum CommandClass {
            RETRIEVAL_COMMAND,
            STORAGE_COMMAND,
            ARITHMETIC_COMMAND,
            TOUCH_COMMAND,
            DELETE_COMMAND,
            SERVICE_COMMAND,
            QUIT_COMMAND,
            SYNC_COMMAND,
            UNDEFINED_COMMAND
        };


        enum Command {
#define MEMCACHED_COMMANDS_ENUM_ELEMENT(cmd, type) cmd,
            MEMCACHED_COMMANDS_ENUM(MEMCACHED_COMMANDS_ENUM_ELEMENT)
#undef MEMCACHED_COMMANDS_ENUM_ELEMENT
        };

        constexpr CommandClass __CommandClasses[] = {
#define MEMCACHED_COMMANDS_ENUM_ELEMENT_TYPE(cmd, type) type,
            MEMCACHED_COMMANDS_ENUM(MEMCACHED_COMMANDS_ENUM_ELEMENT_TYPE)
#undef MEMCACHED_COMMANDS_ENUM_ELEMENT_TYPE
        };


        constexpr CommandClass ClassOfCommand(Command cmd) {
            return __CommandClasses[static_cast<unsigned>(cmd)];
        }


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


#define MEMCACHED_RESPONSES_ENUM_STRELEMENT(resp) bytes::from_literal(CACHELOT_PP_STR(resp)),
        constexpr bytes __AsciiResponses[] = {
            MEMCACHED_RESPONSES_ENUM(MEMCACHED_RESPONSES_ENUM_STRELEMENT)
        };
#undef MEMCACHED_RESPONSES_ENUM_STRELEMENT

        constexpr bytes AsciiResponse(Response r) noexcept {
            return __AsciiResponses[static_cast<unsigned>(r)];
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

#define MEMCACHED_ERRTYPES_ENUM_STRELEMENT(errty) bytes::from_literal(CACHELOT_PP_STR(errty)),
        constexpr bytes __AsciiErrorTypes[] {
            MEMCACHED_ERRTYPES_ENUM(MEMCACHED_ERRTYPES_ENUM_STRELEMENT)
        };
#undef MEMCACHED_ERRTYPES_ENUM_STRELEMENT

        inline constexpr bytes AsciiErrorType(ErrorType et) noexcept {
            return __AsciiErrorTypes[static_cast<unsigned>(et)];
        }


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

    typedef fnv1a<cache::hash_type>::hasher HashFunction;


} } // namespace cachelot::memcached

/// @}

#endif // CACHELOT_PROTO_MEMCACHED_H_INCLUDED

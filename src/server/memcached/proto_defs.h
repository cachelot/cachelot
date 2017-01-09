#ifndef CACHELOT_MEMCACHED_PROTO_DEFS_H_INCLUDED
#define CACHELOT_MEMCACHED_PROTO_DEFS_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file



#ifndef CACHELOT_SLICE_H_INCLUDED
#  include <cachelot/slice.h>
#endif


namespace cachelot { namespace memcached {

#define MEMCACHED_COMMANDS_ENUM(x)  \
        x(ADD)                  \
        x(APPEND)               \
        x(CAS)                  \
        x(DECR)                 \
        x(DELETE)               \
        x(GET)                  \
        x(GETS)                 \
        x(INCR)                 \
        x(PREPEND)              \
        x(REPLACE)              \
        x(SET)                  \
        x(STATS)                \
        x(TOUCH)                \
        x(QUIT)                 \
        x(VERSION)              \
        x(FLUSH_ALL)            \
        x(UNDEFINED)


/**
 * @class doxygen_response
 *
 * Responses to the different commands
 *
 * - STORED: indicates Item was stored
 * - NOT_STORED: indicates Item was *not* stored because conditions for `add` or `replace` haven't met
 * - EXISTS: indicates that Item beign modified by `cas` command had been modified since last cas fetch
 * - NOT_FOUND: indicates that Item was left unmodified because it was not found
 * - DELETED: indicates that Item was deleted by `del` command
 * - TOUCHED: indicates that Item was modified by the `touch` command
 * - NOT_A_RESPONSE: is default (invalid) value
 */

#define MEMCACHED_RESPONSES_ENUM(x) \
        x(STORED)               \
        x(NOT_STORED)           \
        x(EXISTS)               \
        x(NOT_FOUND)            \
        x(DELETED)              \
        x(TOUCHED)              \
        x(NOT_A_RESPONSE)


        /**
         * All the supported commands
         * @ingroup memcached
         */
        enum class Command {
#define MEMCACHED_COMMANDS_ELEMENT(cmd) cmd,
            MEMCACHED_COMMANDS_ENUM(MEMCACHED_COMMANDS_ELEMENT)
#undef MEMCACHED_COMMANDS_ELEMENT
        };

        /**
         * @copydoc doxygen_response
         * @ingroup memcached
         */
        enum class Response {
#define MEMCACHED_RESPONSES_ELEMENT(resp) resp,
            MEMCACHED_RESPONSES_ENUM(MEMCACHED_RESPONSES_ELEMENT)
#undef MEMCACHED_RESPONSES_ELEMENT
        };


#define MEMCACHED_RESPONSES_ENUM_STRELEMENT(resp) slice::from_literal(CACHELOT_PP_STR(resp)),
        constexpr slice __AsciiResponses[] = {
            MEMCACHED_RESPONSES_ENUM(MEMCACHED_RESPONSES_ENUM_STRELEMENT)
        };
#undef MEMCACHED_RESPONSES_ENUM_STRELEMENT

        /**
         * Convert response from the Enum type to the char slice (no zero terminator)
         * @ingroup memcached
         */
        constexpr slice AsciiResponse(Response r) noexcept {
            return __AsciiResponses[static_cast<unsigned>(r)];
        }

}} // namespace cachelot::memcached


#endif // CACHELOT_MEMCACHED_PROTO_DEFS_H_INCLUDED

#ifndef CACHELOT_CACHE_DEFS_H_INCLUDED
#define CACHELOT_CACHE_DEFS_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif

/// @ingroup cache
/// @{

namespace cachelot {

    namespace cache {

#define CACHE_COMMANDS_ENUM(x)  \
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
        x(UNDEFINED)

        /**
         * All the supported commands
         */
        enum Command {
#define CACHE_COMMANDS_ELEMENT(cmd) cmd,
            CACHE_COMMANDS_ENUM(CACHE_COMMANDS_ELEMENT)
#undef CACHE_COMMANDS_ELEMENT
        };


#define CACHE_RESPONSES_ENUM(x) \
        x(STORED)               \
        x(NOT_STORED)           \
        x(EXISTS)               \
        x(NOT_FOUND)            \
        x(DELETED)              \
        x(TOUCHED)              \
        x(NOT_A_RESPONSE)

        /** 
         * Cache responces to the different commands
         *
         * - STORED: indicates Item was stored
         * - NOT_STORED: indicates Item was *not* stored because conditions for `add` or `replace` haven't met
         * - EXISTS: indicates that Item beign modified by `cas` command had been modified since last cas fetch
         * - NOT_FOUND: indicates that Item was left unmodified because it was not found
         * - DELETED: indicates that Item was deleted by `del` command
         * - TOUCHED: indicates that Item was modified by the `touch` command
         */
        enum Response {
#define CACHE_RESPONSES_ELEMENT(resp) resp,
            CACHE_RESPONSES_ENUM(CACHE_RESPONSES_ELEMENT)
#undef CACHE_RESPONSES_ELEMENT
        };


#define CACHE_RESPONSES_ENUM_STRELEMENT(resp) bytes::from_literal(CACHELOT_PP_STR(resp)),
        constexpr bytes __AsciiResponses[] = {
            CACHE_RESPONSES_ENUM(CACHE_RESPONSES_ENUM_STRELEMENT)
        };
#undef CACHE_RESPONSES_ENUM_STRELEMENT

        /// Convert cache response from the Enum type to the ASCII string (without zero terminator)
        constexpr bytes AsciiResponse(Response r) noexcept {
            return __AsciiResponses[static_cast<unsigned>(r)];
        }

}} // namespace cachelot::cache

/// @}

#endif // CACHELOT_CACHE_DEFS_H_INCLUDED

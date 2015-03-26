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

// Primary table of all supported commands and their classes
#define CACHE_COMMANDS_ENUM(x)              \
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
        x(UNDEFINED, UNDEFINED_COMMAND)

        enum Command {
#define CACHE_COMMANDS_ELEMENT(cmd, type) cmd,
            CACHE_COMMANDS_ENUM(CACHE_COMMANDS_ELEMENT)
#undef CACHE_COMMANDS_ELEMENT
        };

        constexpr CommandClass __CommandClasses[] = {
#define CACHE_COMMANDS_ELEMENT_TYPE(cmd, type) type,
            CACHE_COMMANDS_ENUM(CACHE_COMMANDS_ELEMENT_TYPE)
#undef CACHE_COMMANDS_ELEMENT_TYPE
        };


        constexpr CommandClass ClassOfCommand(Command cmd) {
            return __CommandClasses[static_cast<unsigned>(cmd)];
        }


#define CACHE_RESPONSES_ENUM(x) \
        x(STORED)                           \
        x(NOT_STORED)                       \
        x(EXISTS)                           \
        x(NOT_FOUND)                        \
        x(DELETED)                          \
        x(TOUCHED)                          \
        x(NOT_A_RESPONSE)

        /** 
         * Cache responces to different commands
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

        constexpr bytes AsciiResponse(Response r) noexcept {
            return __AsciiResponses[static_cast<unsigned>(r)];
        }

}} // namespace cachelot::cache

/// @}

#endif // CACHELOT_CACHE_DEFS_H_INCLUDED

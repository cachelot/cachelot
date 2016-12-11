//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <cachelot/common.h>
#include <cachelot/stats.h>

#include <iostream>
#include <iomanip>

namespace cachelot {

    struct stats __stats__;

    #define PRINT_STAT(stat_group, stat_type, stat_name, stat_description) \
    std::cout << CACHELOT_PP_STR(stat_group) << ':' << std::setfill('.') << std::setw(40) << std::left << CACHELOT_PP_STR(stat_name) << std::setfill(' ')  << ' ' << std::setw(14) << STAT_GET(stat_group, stat_name) << stat_description << '\n';

    void PrintStats() noexcept {
    // variable tracking size limit exceeded in ASAN build
    #ifndef ADDRESS_SANITIZER
        try {
            #define PRINT_CACHE_STAT(stat_type, stat_name, stat_description) PRINT_STAT(cache, stat_type, stat_name, stat_description)
            CACHE_STATS(PRINT_CACHE_STAT)
            #undef PRINT_CACHE_STAT

            std::cout << std::endl;

            #define PRINT_MEM_STAT(stat_type, stat_name, stat_description) PRINT_STAT(mem, stat_type, stat_name, stat_description)
            MEMORY_STATS(PRINT_MEM_STAT)
            #undef PRINT_MEM_STAT

            std::cout << std::endl;
        } catch (const std::exception &) { /* DO NOTHING */ }
    #endif // ADDRESS_SANITIZER
    }

    #undef PRINT_STAT

    void ResetStats() noexcept {
        new (&__stats__) stats();
    }

} // namespace cachelot


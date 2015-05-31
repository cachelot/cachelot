#include <cachelot/common.h>
#include <cachelot/stats.h>

#include <iostream>
#include <iomanip>

#define __STAT(group, name) __stats__.group.name
#define STAT(group, name) __STAT(group, name)

namespace cachelot {

    void PrintStats() {
#define MEM_STAT_PRINT(field_type, field_name, field_default, field_description) std::cout << std::setfill('.') << std::setw(40) << std::left << #field_name << std::setfill(' ') << std::setw(14) << STAT(mem, field_name) << field_description << std::endl;
            MEMORY_STATS(MEM_STAT_PRINT)
#undef MEM_STAT_PRINT
    }

    struct stats __stats__;

} // namespace cachelot


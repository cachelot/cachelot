#include "unit_test.h"
#include <cachelot/item.h>

namespace {

using namespace cachelot;

static constexpr size_t MEMSIZE = 1024 * 1024 * 4; // 4Mb
static constexpr size_t NUM_ALLOC = 100000;

BOOST_AUTO_TEST_SUITE(test_memalloc)

// allocate and free blocks of a random size
// in case of internal inconsistency, memalloc will trigger debug_assert
//
BOOST_AUTO_TEST_CASE(test_random_allocations) {
    std::unique_ptr<byte[]> _g_memory(new byte[MEMSIZE] );
    memalloc allocator(_g_memory.get(), MEMSIZE);
    for
}


BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


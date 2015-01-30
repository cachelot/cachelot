#include "unit_test.h"
#include <cachelot/memalloc.h>
#include <cachelot/xrange.h>
#include <deque>

namespace {

using namespace cachelot;

static constexpr size_t MEMSIZE = 1024 * 1024 * 64; // 4Mb
static constexpr size_t NUM_ALLOC = 100000;
static constexpr size_t NUM_REPEAT = 5;
static constexpr size_t MIN_ALLOC_SIZE = 4;
static constexpr size_t MAX_ALLOC_SIZE = 1024 * 1024;


BOOST_AUTO_TEST_SUITE(test_memalloc)

// allocate and free blocks of a random size
// in case of internal inconsistency, memalloc will trigger break via debug_assert
//
BOOST_AUTO_TEST_CASE(test_random_allocations) {
    std::unique_ptr<char[]> _g_memory(new char[MEMSIZE] );
    memalloc allocator(_g_memory.get(), MEMSIZE);
    random_int<size_t> random_size(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
    for (size_t repeat_no = 0; repeat_no < NUM_REPEAT; ++repeat_no) {
        random_int<> probability(1, 100);
        std::deque<void * > allocations;
        // random allocations / deallocations
        for (size_t allocation_no = 0; allocation_no < NUM_ALLOC; ++allocation_no) {
            // try to allocate new element
            void * ptr = allocator.alloc(random_size());
            if (ptr != nullptr) {
                allocations.push_back(ptr);
            }
            // delete element with some probability
            if (not allocations.empty() && probability() > 60) { // ~40%
                ptr = allocations.front();
                allocations.pop_front();
                allocator.free(ptr);
            }
        }
        // free previously allocated memory
        for (void * ptr : allocations) {
            allocator.free(ptr);
        }
        allocations.clear();
        // start over again
    }
}


BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


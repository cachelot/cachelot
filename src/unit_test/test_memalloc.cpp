#include "unit_test.h"
#include <cachelot/memalloc.h>
#include <cachelot/xrange.h>


namespace {

using namespace cachelot;

static constexpr size_t MEMSIZE = 1024 * 1024 * 4; // 4Mb
static constexpr size_t NUM_ALLOC = 1000000;
static constexpr size_t NUM_REPEAT = 5;
static constexpr size_t MIN_ALLOC_SIZE = 4;
static constexpr size_t MAX_ALLOC_SIZE = 1024 * 1024;


BOOST_AUTO_TEST_SUITE(test_memalloc)

BOOST_AUTO_TEST_CASE(test_block_list) {
    memalloc::block_list the_list;
    BOOST_CHECK(the_list.empty());
    memalloc::block b1;
    // single item basic operations
    the_list.push_front(&b1);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b1);
    BOOST_CHECK_EQUAL(the_list.back(), &b1);
    BOOST_CHECK(the_list.is_head(&b1));
    BOOST_CHECK(the_list.is_tail(&b1));
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b1);
    BOOST_CHECK(the_list.empty());
    BOOST_CHECK(the_list.front() == nullptr);
    BOOST_CHECK(the_list.back() == nullptr);
    memalloc::block b2;
    // multiple item operations
    the_list.push_front(&b1);
    the_list.push_front(&b2);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b1);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b1));
    // remove one item
    memalloc::block_list::unlink(&b1);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b2);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b2));
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b2);
}


// allocate and free blocks of a random size
// in case of internal inconsistency, memalloc will trigger break via debug_assert
//
BOOST_AUTO_TEST_CASE(memalloc_stress_test) {
    std::unique_ptr<char[]> _g_memory(new char[MEMSIZE] );
    memalloc allocator(_g_memory.get(), MEMSIZE);
    random_int<size_t> random_size(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
    std::vector<void * > allocations;
    allocations.reserve(NUM_ALLOC);
    uint64 num_allocation_errors = 0;

    for (size_t repeat_no = 0; repeat_no < NUM_REPEAT; ++repeat_no) {
        random_int<> probability(1, 100);

        // random allocations / deallocations
        for (size_t allocation_no = 0; allocation_no < NUM_ALLOC; ++allocation_no) {
            // try to allocate new element
            // if we need to evict existing elements to free space, remove it from the allocations list
            void * ptr = allocator.alloc_or_evict(random_size(), true,
                [&allocations](void * mem) {
                    for (size_t i = 0; i < allocations.size(); ++i) {
                        if (allocations[i] == mem) {
                            allocations[i] = allocations.back();
                            allocations.pop_back();
                            return;
                        }
                    }
                    debug_assert(false && "unknown pointer");
                });
            if (ptr != nullptr) {
                allocations.push_back(ptr);
            } else {
                num_allocation_errors += 1;
            }
            // delete element with some probability
            if (not allocations.empty() && probability() > 60) { // ~40%
                random_int<> random_prev_allocation(0, allocations.size() - 1);
                size_t prev_alloc_index = random_prev_allocation();
                ptr = allocations.at(prev_alloc_index);
                BOOST_CHECK(ptr != nullptr);
                allocator.free(ptr);
                // remove pointer from the vector
                allocations[prev_alloc_index] = allocations.back();
                allocations.resize(allocations.size() - 1);
            }
        }
        // free previously allocated memory
        for (void * ptr : allocations) {
            allocator.free(ptr);
        }
        allocations.clear();
        // start over again
    }
    if (num_allocation_errors > 0) {
        BOOST_TEST_MESSAGE("There are " << num_allocation_errors << " failures of " << NUM_ALLOC * NUM_REPEAT << " allocations");
    }
}


BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


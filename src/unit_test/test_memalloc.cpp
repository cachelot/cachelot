#include "unit_test.h"
#include <cachelot/memalloc.h>


namespace {

using namespace cachelot;

static constexpr size_t MEMSIZE = 1024 * 1024 * 4; // 4Mb
static constexpr size_t NUM_ALLOC = 100000;
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
    memalloc::block b2, b3;
    // multiple item operations
    the_list.push_front(&b1);
    the_list.push_front(&b2);
    the_list.push_back(&b3);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove one item
    memalloc::block_list::unlink(&b1);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove second element
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b2);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b3);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b3));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove last element
    memalloc::block_list::unlink(&b3);
    BOOST_CHECK(the_list.empty());
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
            }
            // delete element with some probability
            if (not allocations.empty() && probability() > 60) { // ~40%
                if (repeat_no == NUM_REPEAT - 1) continue;
                random_int<> random_offset(0, allocations.size() - 1);
                auto prev_alloc = allocations.begin() + random_offset();
                BOOST_CHECK(*prev_alloc != nullptr);
                allocator.free(*prev_alloc);
                // remove pointer from the vector
                *prev_alloc = allocations.back();
                allocations.pop_back();
            }
        }
        if (repeat_no == NUM_REPEAT - 1) break;
        // free previously allocated memory
        while (not allocations.empty()) {
            void * ptr = allocations.back();
            allocator.free(ptr);
            allocations.pop_back();
        }
        // start over again
    }
    std::cout << "num_malloc = " << allocator.stats.num_malloc << std::endl;
    std::cout << "num_free = " << allocator.stats.num_free << std::endl;
    std::cout << "num_realloc = " << allocator.stats.num_realloc << std::endl;
    std::cout << "num_errors = " << allocator.stats.num_errors << std::endl;
    std::cout << "total_requested_mem = " << allocator.stats.total_requested_mem << std::endl;
    std::cout << "total_served_mem = " << allocator.stats.total_served_mem << std::endl;
    std::cout << "served_mem = " << allocator.stats.served_mem << std::endl;
    std::cout << "num_free_table_hits = " << allocator.stats.num_free_table_hits << std::endl;
    std::cout << "num_used_table_hits = " << allocator.stats.num_used_table_hits << std::endl;
    std::cout << "num_free_table_splits = " << allocator.stats.num_free_table_splits << std::endl;
    std::cout << "num_used_table_merges = " << allocator.stats.num_used_table_splits << std::endl;
    std::cout << "num_splits = " << allocator.stats.num_splits << std::endl;
    std::cout << "num_merges = " << allocator.stats.num_merges << std::endl;
}


BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


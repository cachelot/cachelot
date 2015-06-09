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
    const size_t blocks_mem_size = (memalloc::block::min_size + memalloc::block::meta_size) * 5;
    std::unique_ptr<uint8[]> blocks_layout(new uint8[blocks_mem_size]);
    uint8 * layout_ptr = blocks_layout.get();
    memalloc::block * left_border = new (layout_ptr) memalloc::block();
    layout_ptr += left_border->size_with_meta();

    memalloc::block * b1 = new (layout_ptr) memalloc::block(memalloc::block::min_size, left_border);
    layout_ptr += b1->size_with_meta();
    memalloc::block * b2 = new (layout_ptr) memalloc::block(memalloc::block::min_size, b1);
    layout_ptr += b2->size_with_meta();
    memalloc::block * b3 = new (layout_ptr) memalloc::block(memalloc::block::min_size, b2);
    layout_ptr += b3->size_with_meta();

    memalloc::block * right_border = new (layout_ptr) memalloc::block(0, b3);
    memalloc::block::checkout(left_border);
    memalloc::block::checkout(right_border);


    memalloc::block_list the_list;
    BOOST_CHECK(the_list.empty());
    // single item basic operations
    the_list.push_front(b1);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), b1);
    BOOST_CHECK_EQUAL(the_list.back(), b1);
    BOOST_CHECK(the_list.is_head(b1));
    BOOST_CHECK(the_list.is_tail(b1));
    BOOST_CHECK_EQUAL(the_list.pop_back(), b1);
    BOOST_CHECK(the_list.empty());
    the_list.push_back(b1);
    BOOST_CHECK_EQUAL(the_list.back(), b1);
    BOOST_CHECK_EQUAL(the_list.front(), b1);
    BOOST_CHECK(the_list.is_head(b1));
    BOOST_CHECK(the_list.is_tail(b1));
    BOOST_CHECK_EQUAL(the_list.pop_front(), b1);
    BOOST_CHECK(the_list.empty());

    // multiple item operations
    the_list.push_front(b1);
    the_list.push_front(b2);
    the_list.push_back(b3);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), b2);
    BOOST_CHECK_EQUAL(the_list.back(), b3);
    BOOST_CHECK(the_list.is_head(b2));
    BOOST_CHECK(the_list.is_tail(b3));
    // remove one item
    memalloc::block_list::unlink(b1);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), b2);
    BOOST_CHECK_EQUAL(the_list.back(), b3);
    BOOST_CHECK(the_list.is_head(b2));
    BOOST_CHECK(the_list.is_tail(b3));
    // remove second element
    BOOST_CHECK_EQUAL(the_list.pop_front(), b2);
    BOOST_CHECK(not the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), b3);
    BOOST_CHECK_EQUAL(the_list.back(), b3);
    BOOST_CHECK(the_list.is_head(b3));
    BOOST_CHECK(the_list.is_tail(b3));
    // remove last element
    memalloc::block_list::unlink(b3);
    BOOST_CHECK(the_list.empty());
}


// allocate and free blocks of a random size
// in case of the internal inconsistency, memalloc will trigger internal failure calling debug_assert
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
}


BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


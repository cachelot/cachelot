#include "unit_test.h"
#include <cachelot/memalloc.h>


namespace {

using namespace cachelot;

// there is no memalloc in the AddressSanitizer build
#ifndef ADDRESS_SANITIZER

BOOST_AUTO_TEST_SUITE(test_memalloc)

// return `true` with probability of given `persents`
inline bool probably(unsigned persents) noexcept {
    debug_assert(persents <= 100);
    random_int<unsigned> chance(1, 100);
    return chance() > (100u - persents);
}


template <class Container>
typename Container::iterator random_choise(Container & c) noexcept {
    random_int<> random_offset(0, c.size() - 1);
    return c.begin() + random_offset();
}


BOOST_AUTO_TEST_CASE(test_free_blocks_by_size) {
    constexpr size_t page_size = 4*Kilobyte;
	memalloc::free_blocks_by_size fixture(page_size);
    // Test position_from_size
    if (memalloc::free_blocks_by_size::first_power_of_2 == 8) {
        // Small blocks (zero cell)
        auto pos = fixture.position_from_size(64);
        BOOST_CHECK_EQUAL(pos.pow_index, 0); BOOST_CHECK_EQUAL(pos.sub_index, 8);
        pos = fixture.position_from_size(63);
        BOOST_CHECK_EQUAL(pos.pow_index, 0); BOOST_CHECK_EQUAL(pos.sub_index, 7);
        pos = fixture.position_from_size(65);
        BOOST_CHECK_EQUAL(pos.pow_index, 0); BOOST_CHECK_EQUAL(pos.sub_index, 8);
        pos = fixture.position_from_size(71);
        BOOST_CHECK_EQUAL(pos.pow_index, 0); BOOST_CHECK_EQUAL(pos.sub_index, 8);
        pos = fixture.position_from_size(255);
        BOOST_CHECK_EQUAL(pos.pow_index, 0); BOOST_CHECK_EQUAL(pos.sub_index, 31);
        // Normal blocks
        pos = fixture.position_from_size(256);
        BOOST_CHECK_EQUAL(pos.pow_index, 1); BOOST_CHECK_EQUAL(pos.sub_index, 0);
        pos = fixture.position_from_size(1026);
        BOOST_CHECK_EQUAL(pos.pow_index, 3); BOOST_CHECK_EQUAL(pos.sub_index, 0);
        pos = fixture.position_from_size(1023);
        BOOST_CHECK_EQUAL(pos.pow_index, 2); BOOST_CHECK_EQUAL(pos.sub_index, 31);
        pos = fixture.position_from_size(2345);
        BOOST_CHECK_EQUAL(pos.pow_index, 4); BOOST_CHECK_EQUAL(pos.sub_index, 4);
    } else if (memalloc::free_blocks_by_size::first_power_of_2 == 7) {
        // TODO: Tests for 32-bit platform
        BOOST_ERROR("Not implemented");
    } else {
        BOOST_ERROR("Unexpected free_blocks_by_size::first_power_of_2");
    }
    // Test try_get_block / next_non_empty
    {
        uint8 _blk1_mem[sizeof(memalloc::block)]; uint8 _blk2_mem[sizeof(memalloc::block)];
        memalloc::block * blk1 = new (_blk1_mem) memalloc::block(128, 0);
        memalloc::block * blk2 = new (_blk2_mem) memalloc::block(128, 0);
        memalloc::block * result_block;
        // small blocks
        blk1->meta.size = 255; fixture.put_block(blk1);
        result_block = fixture.try_get_block(255);
        BOOST_CHECK(result_block == blk1);

        blk1->meta.size = 255; fixture.put_block(blk1);
        blk2->meta.size = 256; fixture.put_block(blk2);
        result_block = fixture.try_get_block(256);
        BOOST_CHECK(result_block == blk2);
        result_block = fixture.try_get_block(123);
        BOOST_CHECK(result_block == blk1);

        blk1->meta.size = 255; fixture.put_block(blk1);
        blk2->meta.size = 1120; fixture.put_block(blk2);
        result_block = fixture.try_get_block(1121);
        BOOST_CHECK(result_block == nullptr);
        result_block = fixture.try_get_block(255);
        BOOST_CHECK(result_block == blk1);
        result_block = fixture.try_get_block(255);
        BOOST_CHECK(result_block == blk2);
        result_block = fixture.try_get_block(255);
        BOOST_CHECK(result_block == nullptr);
    }
}


BOOST_AUTO_TEST_CASE(test_pages) {
    constexpr size_t page_size = 4;
    constexpr size_t arena_size = 16;
    std::unique_ptr<uint8, decltype(&std::free)> memory_arena((uint8 *)aligned_alloc(page_size, arena_size), &std::free);
    uint8 * const arena_begin = memory_arena.get();
    uint8 * const arena_end = memory_arena.get() + arena_size;
    memalloc::pages fixture(4, arena_begin, arena_end);
    BOOST_CHECK_EQUAL(fixture.num_pages, 4);
    // page_info_from_addr
    auto page = fixture.page_info_from_addr(arena_begin + 0);
    BOOST_CHECK(page == &fixture.all_pages[0]);
    page = fixture.page_info_from_addr(arena_begin + 4);
    BOOST_CHECK(page == &fixture.all_pages[1]);
    page = fixture.page_info_from_addr(arena_begin + 7);
    BOOST_CHECK(page == &fixture.all_pages[1]);
    page = fixture.page_info_from_addr(arena_begin + 15);
    BOOST_CHECK(page == &fixture.all_pages[3]);
    // page_boundaries_from_addr
    const uint8 * page_beg; const uint8 * page_end;
    tie(page_beg, page_end) = fixture.page_boundaries_from_addr(arena_begin + 0);
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 0));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 4));
    tie(page_beg, page_end) = fixture.page_boundaries_from_addr(arena_begin + 4);
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 4));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 8));
    tie(page_beg, page_end) = fixture.page_boundaries_from_addr(arena_begin + 14);
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 12));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 16));
    tie(page_beg, page_end) = fixture.page_boundaries_from_addr(arena_begin + 15);
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 12));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 16));
    // touch
    BOOST_CHECK_EQUAL(fixture.all_pages[0].num_hits, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[1].num_hits, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[2].num_hits, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[3].num_hits, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[0].num_evictions, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[1].num_evictions, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[2].num_evictions, 0);
    BOOST_CHECK_EQUAL(fixture.all_pages[3].num_evictions, 0);
    fixture.touch(arena_begin + 0);
    fixture.touch(arena_begin + 1);
    BOOST_CHECK_EQUAL(fixture.all_pages[0].num_hits, 2);
    fixture.touch(arena_begin + 15);
    BOOST_CHECK_EQUAL(fixture.all_pages[3].num_hits, 1);
    fixture.touch(arena_begin + 9);
    BOOST_CHECK_EQUAL(fixture.all_pages[2].num_hits, 1);
    // page_to_reuse
    tie(page_beg, page_end) = fixture.page_to_reuse();
    //      must be untouched page #1
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 4));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 8));
    BOOST_CHECK_EQUAL(&fixture.all_pages[1], fixture.lru_pages.front());
    BOOST_CHECK_EQUAL(fixture.all_pages[1].num_evictions, 1);
    //      touch all pages except #0
    for (uint8 * addr = arena_begin + 4; addr < arena_end; ++addr) {
        fixture.touch(addr);
    }
    tie(page_beg, page_end) = fixture.page_to_reuse();
    BOOST_CHECK_EQUAL((const void *)page_beg, (const void *)(arena_begin + 0));
    BOOST_CHECK_EQUAL((const void *)page_end, (const void *)(arena_begin + 4));
    BOOST_CHECK_EQUAL(&fixture.all_pages[0], fixture.lru_pages.front());
    BOOST_CHECK_EQUAL(fixture.all_pages[0].num_evictions, 1);
}

BOOST_AUTO_TEST_CASE(test_realloc_inplace) {
    // setup
    memalloc allocator(4 * Kilobyte, 1 * Kilobyte);
    const auto less_than_halfpage = 300u;
    // allocate < ~30% of the page
    void * mem1 = allocator.alloc(less_than_halfpage);
    BOOST_CHECK(mem1 != nullptr);
    std::memset(mem1, 'X', less_than_halfpage); // "use" memory
    // allocate < ~30% more
    void * mem2 = allocator.alloc(less_than_halfpage);
    BOOST_CHECK(mem2 != nullptr);
    std::memset(mem2, 'X', less_than_halfpage); // "use" memory
    // allocate the same
    mem2 = allocator.realloc_inplace(mem2, less_than_halfpage);
    BOOST_CHECK(mem2 != nullptr);
    std::memset(mem2, 'X', less_than_halfpage); // "use" memory
    // shrink first chunk
    mem1 = allocator.realloc_inplace(mem1, less_than_halfpage / 2);
    BOOST_CHECK(mem1 != nullptr);
    std::memset(mem1, 'X', less_than_halfpage / 2); // "use" memory
    // shrink some more
    mem1 = allocator.realloc_inplace(mem1, less_than_halfpage / 4);
    BOOST_CHECK(mem1 != nullptr);
    std::memset(mem1, 'X', less_than_halfpage / 4); // "use" memory
    // now extend the first chunk back
    mem1 = allocator.realloc_inplace(mem1, less_than_halfpage);
    BOOST_CHECK(mem1 != nullptr);
    std::memset(mem1, 'X', less_than_halfpage); // "use" memory
    // extend a second chunk a bit
    mem2 = allocator.realloc_inplace(mem2, less_than_halfpage + 1);
    BOOST_CHECK(mem2 != nullptr);
    std::memset(mem2, 'X', less_than_halfpage + 1); // "use" memory
    // delete the second chunk
    allocator.free(mem2);
    // try to extend the first
    mem1 = allocator.realloc_inplace(mem1, less_than_halfpage * 2);
    BOOST_CHECK(mem1 != nullptr);
    std::memset(mem1, 'X', less_than_halfpage * 2); // "use" memory
}

BOOST_AUTO_TEST_CASE(memalloc_basic_stats) {
    ResetStats();
    memalloc allocator(Megabyte, Kilobyte);
    // check constant stats
    BOOST_CHECK_EQUAL(STAT_GET(mem,limit_maxbytes), Megabyte);
    BOOST_CHECK_EQUAL(STAT_GET(mem,page_size), Kilobyte);
    // check initial stat values
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_malloc), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_free), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_realloc), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_alloc_errors), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_realloc_errors), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_requested), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_served), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_unserved), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_requested), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_served), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_unserved), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_free_table_hits), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_free_table_weak_hits), 0u);
    BOOST_CHECK_EQUAL(STAT_GET(mem,evictions), 0u);

}

// allocate and free blocks of a random size
// in case of the internal inconsistency, memalloc will trigger internal failure calling debug_assert
//
BOOST_AUTO_TEST_CASE(memalloc_stress_test) {
    static constexpr size_t MEM_SIZE = 4 * Megabyte;
    static constexpr size_t MEM_PAGE_SIZE = 4 * Kilobyte;
    static constexpr size_t NUM_ALLOC = 100000;
    static constexpr size_t NUM_REPEAT = 50;
    static constexpr size_t MIN_ALLOC_SIZE = 4;
    static constexpr size_t MAX_ALLOC_SIZE = MEM_PAGE_SIZE - 64;
    // setup
    random_int<size_t> random_size(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
    std::vector<void * > allocations;
    allocations.reserve(NUM_ALLOC);
    // create memalloc
    memalloc allocator(MEM_SIZE, MEM_PAGE_SIZE);

    ResetStats();
    uint64 my_num_malloc = 0u;
    uint64 my_num_free = 0u;
    uint64 my_num_realloc = 0u;
    uint64 my_num_alloc_errors = 0u;
    uint64 my_num_realloc_errors = 0u;
    uint64 my_used_memory = 0u;
    uint64 my_total_requested = 0u;
    uint64 my_total_served = 0u;
    uint64 my_total_unserved = 0u;
    uint64 my_total_realloc_requested = 0u;
    uint64 my_total_realloc_served = 0u;
    uint64 my_total_realloc_unserved = 0u;
    uint64 my_evictions = 0u;

    auto do_alloc = [&](const size_t allocation_size) -> void * {
            my_num_malloc += 1;
            my_total_requested += allocation_size;
            void * ptr = allocator.alloc_or_evict(allocation_size, true,
                [&](void * mem) {
                    my_evictions += 1;
                    const auto mem_full_size = allocator.reveal_actual_size(mem);
                    debug_assert(my_used_memory >= mem_full_size);
                    my_used_memory -= mem_full_size;
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
                const auto mem_full_size = allocator.reveal_actual_size(ptr);
                my_total_served += mem_full_size;
                debug_assert(STAT_GET(mem,total_served) == my_total_served);
                my_used_memory += mem_full_size;
            } else {
                my_num_alloc_errors += 1;
                my_total_unserved += allocation_size;
            }
            return ptr;
    };

    auto do_free = [&](void * ptr) {
        BOOST_CHECK(ptr != nullptr);
        my_num_free += 1;
        const auto mem_full_size = allocator.reveal_actual_size(ptr);
        BOOST_CHECK(my_used_memory >= mem_full_size);
        my_used_memory -= mem_full_size;
        allocator.free(ptr);
    };


    auto do_realloc= [&](void * ptr, const size_t new_size) {
        BOOST_CHECK(ptr != nullptr);
        my_num_realloc += 1;
        // store old size
        const auto old_full_size = allocator.reveal_actual_size(ptr);
        const auto old_size = old_full_size - memalloc::header_size();
        if (old_size < new_size) {
            my_total_realloc_requested += (new_size - old_size);
        }
        my_used_memory -= old_full_size;
        // try to realloc
        auto new_alloc = allocator.realloc_inplace(ptr, new_size);
        const auto new_full_size = allocator.reveal_actual_size(ptr);
        my_used_memory += new_full_size;

        if (new_alloc != nullptr) {
            // suceeded
            if (new_full_size > old_full_size) {
                my_total_realloc_served += new_full_size - old_full_size;
            }
        } else {
            // failed
            my_num_realloc_errors += 1;
            my_total_realloc_unserved += new_size - old_size;
        }
    };

    // run test NUM_REPEAT times
    for (size_t repeat_no = 0; repeat_no < NUM_REPEAT; ++repeat_no) {

        // random allocations / deallocations
        for (size_t allocation_no = 0; allocation_no < NUM_ALLOC; ++allocation_no) {
            // try to allocate new element
            // if we need to evict existing elements to free space, remove it from the allocations list
            const auto allocation_size = random_size();

            // make a new allocation
            void * ptr = do_alloc(allocation_size);
            allocations.push_back(ptr);
            std::memset(ptr, 'X', allocation_size);

            // free one of previously allocated blocks with 40% probability
            if (not allocations.empty() && probably(40)) {
                auto prev_alloc = random_choise(allocations);
                do_free(*prev_alloc);
                // remove pointer from the vector
                *prev_alloc = allocations.back();
                allocations.pop_back();
            }

            // reallocate one of previously allocated blocks with 60% probability
            if (not allocations.empty() && probably(60)) {
                auto prev_alloc = random_choise(allocations);
                const auto new_size = random_size();
                do_realloc(*prev_alloc, new_size);
            }
        }
        // free all previously allocated memory
        while (not allocations.empty()) {
            void * ptr = allocations.back();
            do_free(ptr);
            allocations.pop_back();
        }
        // start over again
    }
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_malloc), my_num_malloc);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_free), my_num_free);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_realloc), my_num_realloc);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_alloc_errors), my_num_alloc_errors);
    BOOST_CHECK_EQUAL(STAT_GET(mem,num_realloc_errors), my_num_realloc_errors);
    BOOST_CHECK_EQUAL(STAT_GET(mem,used_memory), my_used_memory);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_requested), my_total_requested);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_served), my_total_served);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_unserved), my_total_unserved);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_requested), my_total_realloc_requested);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_served), my_total_realloc_served);
    BOOST_CHECK_EQUAL(STAT_GET(mem,total_realloc_unserved), my_total_realloc_unserved);
    BOOST_CHECK_EQUAL(STAT_GET(mem,evictions), my_evictions);
    PrintStats();
}


BOOST_AUTO_TEST_SUITE_END()

#endif // ifndef ADDRESS_SANITIZER

} // anonymouse namespace


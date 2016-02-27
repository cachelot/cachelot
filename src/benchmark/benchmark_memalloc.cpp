#include <cachelot/common.h>
#include <cachelot/memalloc.h>
#include <cachelot/random.h>

#include <iostream>
#include <iomanip>

extern "C" {
#include "tlsf.h"
}


using namespace cachelot;


static const int num_runs = 10;
static const size_t memory_limit = 1024 * Megabyte;
static const size_t page_size = 4 * Kilobyte;
static const size_t user_available_memory = memory_limit - memory_limit * .1; // 90%
static const size_t min_allocation_size = 4;
static const size_t max_allocation_size = page_size - 16;
static const size_t max_allocations_num = memory_limit / min_allocation_size;
typedef std::chrono::high_resolution_clock hires_clock;

int main() {
    size_t total_allocated = 0;
    size_t num_allocations = 0;
    size_t num_allocation_errors = 0;
    std::vector<std::pair<size_t, void *>> allocations;
    std::vector<size_t> allocation_sizes;

    allocations.reserve(max_allocations_num);
    allocation_sizes.reserve(max_allocations_num);
    for (int run = 0; run < num_runs; ++run) {
        std::cout << "### Running test: " << run << std::endl << std::endl;
        static random_int<size_t> calc_rand_size(min_allocation_size, max_allocation_size);
        allocation_sizes.clear();
        for (size_t alloc_no = 0; alloc_no < max_allocations_num; ++alloc_no) {
            allocation_sizes.push_back(calc_rand_size());
        }
////////////////////////////////////////////////////////////////
    std::cout << "## memalloc   ";
////////////////////////////////////////////////////////////////
    {
        memalloc ma(memory_limit, page_size);
        total_allocated = 0;
        num_allocations = 0;
        num_allocation_errors = 0;
        allocations.clear();

        auto start = hires_clock::now();
        for (auto alloc_size : allocation_sizes) {
            if (total_allocated < user_available_memory) {
                void * ptr = ma.alloc(alloc_size);
                if (ptr) {
                    total_allocated += alloc_size;
                    num_allocations += 1;
                    allocations.push_back(std::make_pair(alloc_size, ptr));
                } else {
                    num_allocation_errors += 1;
                }
            } else {
                break;
            }
        }
        for (const auto al : allocations) { ma.free(al.second); }
        auto end = hires_clock::now();
        uint64 took_ns = std::chrono::duration_cast<std::chrono::nanoseconds>((end - start)).count();
        uint64 APS = 1000000000 / (took_ns / (num_allocations+1));
		std::cout << "Num allocations: " << num_allocations << " (" << num_allocation_errors << ") ";
		std::cout << "took: " << std::setfill('0') << std::setw(15) << took_ns << "ns (" << std::setw(10) << APS << " Alloc/sec)" << std::endl;
	}
////////////////////////////////////////////////////////////////
    std::cout << "## OS runtime ";
////////////////////////////////////////////////////////////////
    {
        total_allocated = 0;
        num_allocations = 0;
        allocations.clear();

        auto start = hires_clock::now();
        for (auto alloc_size : allocation_sizes) {
            if (total_allocated < user_available_memory) {
                void * ptr = ::malloc(alloc_size);
                if (ptr) {
                    total_allocated += alloc_size;
                    num_allocations += 1;
                    allocations.push_back(std::make_pair(alloc_size, ptr));
                } else {
                    num_allocation_errors += 1;
                }
            } else {
                break;
            }
        }
        for (const auto al : allocations) { ::free(al.second); }
        auto end = hires_clock::now();
        uint64 took_ns = std::chrono::duration_cast<std::chrono::nanoseconds>((end - start)).count();
        uint64 APS = 1000000000 / (took_ns / (num_allocations+1));
		std::cout << "Num allocations: " << num_allocations << " (" << num_allocation_errors << ") ";
		std::cout << "took: " << std::setfill('0') << std::setw(15) << took_ns << "ns (" << std::setw(10) << APS << " Alloc/sec)" << std::endl;
    }
////////////////////////////////////////////////////////////////
    std::cout << "## TLSF       ";
////////////////////////////////////////////////////////////////
    {
        total_allocated = 0;
        num_allocations = 0;
        allocations.clear();
        std::unique_ptr<uint8[]> arena(new uint8 [memory_limit]);
        init_memory_pool(memory_limit, arena.get());

        auto start = hires_clock::now();
        for (auto alloc_size : allocation_sizes) {
            if (total_allocated < user_available_memory) {
                void * ptr = tlsf_malloc(alloc_size);
                if (ptr) {
                    total_allocated += alloc_size;
                    num_allocations += 1;
                    allocations.push_back(std::make_pair(alloc_size, ptr));
                } else {
                    num_allocation_errors += 1;
                }
            } else {
                break;
            }
        }
        for (const auto al : allocations) { tlsf_free(al.second); }
        auto end = hires_clock::now();
        uint64 took_ns = std::chrono::duration_cast<std::chrono::nanoseconds>((end - start)).count();
        uint64 APS = 1000000000 / (took_ns / (num_allocations+1));
		std::cout << "Num allocations: " << num_allocations << " (" << num_allocation_errors << ") ";
		std::cout << "took: " << std::setfill('0') << std::setw(15) << took_ns << "ns (" << std::setw(10) << APS << " Alloc/sec)" << std::endl;
    }
}
    return 0;
}


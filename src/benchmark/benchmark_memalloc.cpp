#include <cachelot/common.h>
#include <cachelot/memalloc.h>
#include <cachelot/random.h>

#include <iostream>
#include <iomanip>
#include <boost/io/ios_state.hpp>

extern "C" {
#include "tlsf.h"
}


using namespace cachelot;
using namespace std::chrono;


static const int num_runs = 10;
static const size_t memory_limit = 1024 * Megabyte;
static const size_t page_size = 4 * Kilobyte;
static const size_t user_available_memory = memory_limit - memory_limit * .05; // 95%
static const size_t min_allocation_size = 4;
static const size_t max_allocation_size = page_size - 16;
static const size_t max_allocations_num = memory_limit / min_allocation_size;
typedef std::chrono::high_resolution_clock hires_clock;


// array of size-pointer pairs
typedef std::vector<std::pair<size_t, void *>> SizePointerPairs;
// sizes are pre-generated random numbers to call the allocator's malloc(size)
SizePointerPairs global_allocations;

// array of pointers in the `global_allocations` array
// used to deallocate items in random order
std::vector<const std::pair<size_t, void *> *> global_allocations_in_random_order;

void generate_global_test_data() {
    // prepare arrays
    global_allocations.clear();
    global_allocations_in_random_order.clear();
    global_allocations.reserve(max_allocations_num);
    global_allocations_in_random_order.reserve(max_allocations_num);

    random_int<size_t> calc_rand_size(min_allocation_size, max_allocation_size);
    // Fill-in test data with random memory sizes to not waste time on random during the actual benchmark
    size_t total_size = 0;
    for (size_t alloc_no = 0; alloc_no < max_allocations_num; ++alloc_no) {
        auto allocation_size = calc_rand_size();
        if (total_size + allocation_size <= user_available_memory) {
            global_allocations.push_back(std::make_pair(allocation_size, nullptr));
            global_allocations_in_random_order.push_back(&global_allocations[alloc_no]);
            total_size += allocation_size;
        } else {
            // we've reached memory limit
            break;
        }
    }
    std::random_shuffle(global_allocations_in_random_order.begin(), global_allocations_in_random_order.end());
}



struct benchmark_results {
    nanoseconds malloc_took_ns;
    nanoseconds free_took_ns;
    size_t num_allocation_errors = 0;

    float relative_to_baseline(nanoseconds baseline) const noexcept {
        auto total = malloc_took_ns + free_took_ns;
        return (float)baseline.count() / (total.count() + 1); // avoid div by zero
    }
};

template <class AllocFunc, class FreeFunc>
benchmark_results run_benchmark(AllocFunc alloc_func, FreeFunc free_func) {
    benchmark_results results;
    // call allocate function for each size in test_data array
    {
        auto time_start = hires_clock::now();
        for (auto & alloc : global_allocations) {
            alloc.second = alloc_func(alloc.first);
            if (alloc.second != nullptr) {
                continue;
            } else {
                results.num_allocation_errors += 1;
            }
        }
        auto time_end = hires_clock::now();
        results.malloc_took_ns = duration_cast<nanoseconds>(time_end - time_start);
    }

    // call free function
    {
        auto time_start = hires_clock::now();
        for (auto al : global_allocations_in_random_order) {
            if (al->second != nullptr) {
                free_func(al->second);
            }
        }
        auto time_end = hires_clock::now();
        results.free_took_ns = duration_cast<nanoseconds>(time_end - time_start);
    }
    return results;
}

using namespace std;

int main() {
    for (int run = 0; run < num_runs; ++run) {
        // fill-in test data with random values
        generate_global_test_data();

        // C runtime builtin alloc
        auto c_runtime_results = run_benchmark(::malloc, ::free);
        memalloc cachelot_allocator(memory_limit, page_size);
        // cachelot
        auto do_alloc = [&cachelot_allocator](size_t size) -> void * { return cachelot_allocator.alloc(size); };
        auto do_free = [&cachelot_allocator](void * ptr) { cachelot_allocator.free(ptr); };
        auto cachelot_results = run_benchmark(do_alloc, do_free);
        // TLSF
        std::unique_ptr<uint8[]> arena(new uint8 [memory_limit]);
        init_memory_pool(memory_limit, arena.get());
        auto tlsf_results = run_benchmark(tlsf_malloc, tlsf_free);

        // Set baseline as C runtime
        auto baseline = c_runtime_results.malloc_took_ns + c_runtime_results.free_took_ns;

        // Output results
        // Header
        cout << "\n Run #" << setfill('0') << setw(2) << run + 1 << setfill(' ');
        cout << "     "  << setw(10) << global_allocations.size() << " allocations" << endl;
        auto print_results = [=](const char * allocator_name, const benchmark_results res) {
            boost::io::ios_all_saver  __save_state(cout);
            cout << setw(25) << setfill('.') << left << allocator_name << setfill(' ') << right;
            cout << "    alloc: " << setw(8) << setfill('0') << setprecision(6) << (float)res.malloc_took_ns.count() / 1000000 << setfill(' ') << " ms";
            cout << "     free: " << setw(8) << setfill('0') << setprecision(6) << (float)res.free_took_ns.count() / 1000000 << setfill(' ') << " ms";
            cout << setw(10) << "x" << setprecision(3) << res.relative_to_baseline(baseline) << std::endl;
        };
        print_results("runtime builtin", c_runtime_results);
        print_results("cachelot", cachelot_results);
        print_results("TLSF", tlsf_results);
        cout << endl;
    }
    return 0;
}


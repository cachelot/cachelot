#include <cachelot/common.h>
#include <cachelot/memalloc.h>
#include <cachelot/random.h>


using namespace cachelot;


static const int num_runs = 10;
static const size_t buffer_size = 1024*1024*1024;
static const size_t user_available_memory = buffer_size - buffer_size * .2; // 80%
static const size_t min_allocation_size = 4;
static const size_t max_allocation_size = 4096;
static const size_t max_allocations_num = buffer_size / min_allocation_size;
typedef std::chrono::high_resolution_clock hires_clock;

static std::unique_ptr<uint8[]> arena(new uint8[buffer_size]);
static memalloc ma(arena.get(), buffer_size);

size_t total_allocated = 0;
size_t num_allocations = 0;
std::vector<std::pair<size_t, void *>> allocations;
std::vector<size_t> allocation_sizes;


int main() {
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
    std::cout << "## memalloc " << std::endl << std::endl;
////////////////////////////////////////////////////////////////
    {
        total_allocated = 0;
        num_allocations = 0;
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
                    break;
                }
            } else {
                break;
            }
        }
        for (const auto al : allocations) { ma.free(al.second); }
        auto end = hires_clock::now();
        uint64 took_ns = std::chrono::duration_cast<std::chrono::nanoseconds>((end - start)).count();
        uint64 APS = 1000000000 / (took_ns / num_allocations);
        std::cout << "Num allocations: " << num_allocations << " took: " << took_ns << "ns (" << APS << " APS)" << std::endl;
    }
////////////////////////////////////////////////////////////////
    std::cout << "## OS runtime " << std::endl << std::endl;
////////////////////////////////////////////////////////////////
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
                    break;
                }
            } else {
                break;
            }
        }
        for (const auto al : allocations) { ::free(al.second); }
        auto end = hires_clock::now();
        uint64 took_ns = std::chrono::duration_cast<std::chrono::nanoseconds>((end - start)).count();
        uint64 APS = 1000000000 / (took_ns / num_allocations);
        std::cout << "Num allocations: " << num_allocations << " took: " << took_ns << "ns (" << APS << " APS)" << std::endl;
}
    return 0;
}


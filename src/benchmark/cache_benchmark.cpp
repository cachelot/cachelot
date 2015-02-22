#include <cachelot/cachelot.h>
#include <cachelot/cache.h>
#include <cachelot/random.h>
#include <cachelot/hash_fnv1a.h>

#include <iostream>
#include <iomanip>

using namespace cachelot;

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

constexpr size_t num_items = 1000000;
constexpr size_t cache_memory = 64 * 1024 * 1024;
constexpr size_t hash_initial = 131072;
constexpr uint8 min_key_len = 14;
constexpr uint8 max_key_len = 40;
constexpr uint32 min_value_len = 14;
constexpr uint32 max_value_len = 40;

/// Hash function
static auto calc_hash = fnv1a<cache::Cache::hash_type>::hasher();


static struct stats_type {
    uint64 num_get = 0;
    uint64 num_set = 0;
    uint64 num_del = 0;
    uint64 num_cache_hit = 0;
    uint64 num_cache_miss = 0;
    uint64 num_error = 0;
} stats;

inline void reset_stats() {
    new (&stats)stats_type();
}

typedef std::tuple<string, string> kv_type;
typedef std::vector<kv_type> array_type;
typedef array_type::const_iterator iterator;

constexpr cache::seconds forever = cache::seconds(0);

class CacheWrapper {
public:
    CacheWrapper() : m_cache(cache_memory, hash_initial) {}

    void set(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        bytes v (std::get<1>(*it).c_str(), std::get<1>(*it).size());
        m_cache.do_set(k, calc_hash(k), v, 0, forever, 0,
                       [=](error_code error, bool /*stored*/) {
                           stats.num_set += 1;
                           if (error) {
                               stats.num_error += 1;
                           }
                       });
    }

    void get(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        m_cache.do_get(k, calc_hash(k),
                    [=](error_code error, bool found, bytes, cache::opaque_flags_type, cache::cas_value_type) {
                        stats.num_get += 1;
                        if (not error) {
                            auto & counter = found ? stats.num_cache_hit : stats.num_cache_miss;
                            counter += 1;
                        } else {
                            stats.num_error += 1;
                        }
                    });
    }

    void del(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        m_cache.do_del(k, calc_hash(k),
                       [=](error_code error, bool deleted) {
                           stats.num_del += 1;
                           if (not error) {
                               auto & counter = deleted ? stats.num_cache_hit : stats.num_cache_miss;
                               counter += 1;
                           } else {
                               stats.num_error += 1;
                           }
                       });
    }
private:
    cache::Cache m_cache;
};


array_type data_array;
CacheWrapper csh;

inline iterator random_pick() {
    static random_int<array_type::size_type> rndelem(0, data_array.size() - 1);
    auto at = rndelem();
    return data_array.begin() + at;
}


static void generate_test_data() {
    data_array.reserve(num_items);
    for (auto n=num_items; n > 0; --n) {
        kv_type kv(move(random_string(min_key_len, max_key_len)),
                   move(random_string(min_value_len, max_value_len)));
        data_array.emplace_back(move(kv));
    }
}


static void warmup() {
    for (auto n=hash_initial; n > 0; --n) {
        csh.set(random_pick());
    }
}


auto chance = random_int<size_t>(1, 100);

int main(int /*argc*/, char * /*argv*/[]) {
    generate_test_data();
    warmup();
    reset_stats();
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i=0; i<3; ++i) {
        for (iterator kv = data_array.begin(); kv < data_array.end(); ++kv) {
            csh.set(kv);
            if (chance() > 70) {
                csh.del(random_pick());
            }
            if (chance() > 30) {
                csh.get(random_pick());
            }
        }
    }
    auto time_passed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time);
    const double sec = time_passed.count() / 1000000;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Time spent: " << sec << "s" << std::endl;
    std::cout << "get:        " << stats.num_get << std::endl;
    std::cout << "set:        " << stats.num_set << std::endl;
    std::cout << "del:        " << stats.num_del << std::endl;
    std::cout << "cache_hit:  " << stats.num_cache_hit << std::endl;
    std::cout << "cache_miss: " << stats.num_cache_miss << std::endl;
    std::cout << "error:      " << stats.num_error << std::endl;
    const double RPS = (stats.num_get + stats.num_set + stats.num_del) / sec;
    std::cout << "rps:        " << RPS << std::endl;
    std::cout << "avg. cost:  " << static_cast<unsigned>(1000000000 / RPS) << "us" << std::endl;

    return 0;
}

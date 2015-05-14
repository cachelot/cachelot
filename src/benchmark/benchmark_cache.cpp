#include <cachelot/common.h>
#include <cachelot/cache.h>
#include <cachelot/random.h>
#include <cachelot/hash_fnv1a.h>
#include <cachelot/settings.h>
#include <cachelot/stats.h>

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

// Hash function
static auto calc_hash = fnv1a<cache::Cache::hash_type>::hasher();


static struct stats_type {
    uint64 num_get = 0;
    uint64 num_set = 0;
    uint64 num_del = 0;
    uint64 num_cache_hit = 0;
    uint64 num_cache_miss = 0;
    uint64 num_error = 0;
} __stats__;

inline void reset_stats() {
    new (&__stats__)stats_type();
}

typedef std::tuple<string, string> kv_type;
typedef std::vector<kv_type> array_type;
typedef array_type::const_iterator iterator;

const auto forever = cache::expiration_time_point::max();

class CacheWrapper {
public:
    CacheWrapper() : m_cache() {}

    void set(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        bytes v (std::get<1>(*it).c_str(), std::get<1>(*it).size());
        error_code error; cache::ItemPtr item;
        tie(error, item) = m_cache.create_item(k, calc_hash(k), v.length(), /*flags*/0, forever, /*CAS*/0);
        if (not error) {
            item->assign_value(v);
            cache::Response cache_reply;
            tie(error, cache_reply) = m_cache.do_set(item);
        }
        if (not error) {
            __stats__.num_set += 1;
        } else {
            __stats__.num_error += 1;
        }
    }

    void get(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        m_cache.do_get(k, calc_hash(k),
                    [=](error_code error, bool found, bytes, cache::opaque_flags_type, cache::version_type) {
                        __stats__.num_get += 1;
                        if (not error) {
                            auto & counter = found ? __stats__.num_cache_hit : __stats__.num_cache_miss;
                            counter += 1;
                        } else {
                            __stats__.num_error += 1;
                        }
                    });
    }

    void del(iterator it) {
        bytes k (std::get<0>(*it).c_str(), std::get<0>(*it).size());
        error_code error; cache::Response cache_reply;
        tie(error, cache_reply) = m_cache.do_del(k, calc_hash(k));
        __stats__.num_del += 1;
        if (not error) {
            auto & counter = (cache_reply == cache::DELETED) ? __stats__.num_cache_hit : __stats__.num_cache_miss;
            counter += 1;
        } else {
            __stats__.num_error += 1;
        }

    }
private:
    cache::Cache m_cache;
};


array_type data_array;
std::unique_ptr<CacheWrapper> csh;

inline iterator random_pick() {
    debug_assert(data_array.size() > 0);
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
        csh->set(random_pick());
    }
}


auto chance = random_int<size_t>(1, 100);

int main(int /*argc*/, char * /*argv*/[]) {
    // setup cache
    settings.cache.memory_limit = cache_memory;
    settings.cache.initial_hash_table_size = hash_initial;
    csh.reset(new CacheWrapper());
    generate_test_data();
    warmup();
    reset_stats();
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i=0; i<3; ++i) {
        for (iterator kv = data_array.begin(); kv < data_array.end(); ++kv) {
            csh->set(kv);
            if (chance() > 70) {
                csh->del(random_pick());
            }
            if (chance() > 30) {
                csh->get(random_pick());
            }
        }
    }
    auto time_passed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start_time);
    const double sec = time_passed.count() / 1000000000;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Time spent: " << sec << "s" << std::endl;
    std::cout << "get:        " << __stats__.num_get << std::endl;
    std::cout << "set:        " << __stats__.num_set << std::endl;
    std::cout << "del:        " << __stats__.num_del << std::endl;
    std::cout << "cache_hit:  " << __stats__.num_cache_hit << std::endl;
    std::cout << "cache_miss: " << __stats__.num_cache_miss << std::endl;
    std::cout << "error:      " << __stats__.num_error << std::endl;
    const double RPS = (__stats__.num_get + __stats__.num_set + __stats__.num_del) / sec;
    std::cout << "rps:        " << RPS << std::endl;
    std::cout << "avg. cost:  " << static_cast<unsigned>(1000000000 / RPS) << "ns" << std::endl;
    std::cout << std::endl;
    PrintStats();

    return 0;
}

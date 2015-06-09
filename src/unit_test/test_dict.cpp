#include "unit_test.h"
#include <cachelot/dict.h>
#include <cachelot/random.h>
#include <unordered_map>
#include <functional>

namespace {

using namespace cachelot;

typedef dict<string, string> dict_type;

BOOST_AUTO_TEST_SUITE(test_dict)

// insert several random elements into dict and std::unordered_map
// and check their contents are equal
BOOST_AUTO_TEST_CASE(test_dict_basic) {
    static const size_t num_elements = 100000;
    std::unordered_map<string, string> stock_map;
    dict_type the_dict;
    std::hash<string> hasher;
    // fill the tables
    for (uint i = 0; i < num_elements; ++i) {
        string key = random_string(14, 45);
        string value = random_string(4, 400);
        stock_map.insert(std::make_pair(key, value));
        bool found; dict_type::iterator at; auto hash = hasher(key);
        tie(found, at) = the_dict.entry_for(key, hash);
        BOOST_CHECK(not found);
        the_dict.insert(at, key, hash, value);
        BOOST_CHECK(the_dict.contains(key, hash));
    }
    // check
    BOOST_CHECK_EQUAL(the_dict.size(), num_elements);
    BOOST_CHECK_EQUAL(the_dict.size(), stock_map.size());
    for (const auto & kv : stock_map) {
        const string & key = kv.first;
        auto hash = hasher(key);
        BOOST_CHECK(the_dict.contains(key, hash));
        bool found; dict_type::mapped_type dict_value;
        std::tie(found, dict_value) = the_dict.get(key, hash);
        BOOST_CHECK(found);
        BOOST_CHECK_EQUAL(kv.second, dict_value);
        BOOST_CHECK(the_dict.del(key, hash));
        BOOST_CHECK(not the_dict.contains(key, hash));
    }
    BOOST_CHECK_EQUAL(the_dict.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

}



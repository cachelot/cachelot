#include "unit_test.h"
#include <cachelot/xrange.h>
#include <cachelot/hash_table.h>

namespace {

using namespace cachelot;

typedef hash_table<string, void *, std::equal_to<string>> table_type;
typedef table_type::hash_type hash_type;

BOOST_AUTO_TEST_SUITE(test_hash_table)


// TODO: entry_for / insert / remove
BOOST_AUTO_TEST_CASE(test_hash_table_low_level_operations) {


}


// test operations add / insert / get / replace  / del
BOOST_AUTO_TEST_CASE(test_hash_table_operations) {
    static const size_t DefaultCapacity = 16;
    table_type the_dict(DefaultCapacity);
    // check initial state
    BOOST_CHECK(the_dict.empty());
    BOOST_CHECK(the_dict.capacity() == DefaultCapacity);
    BOOST_CHECK(the_dict.size() == 0);
    for (auto i : xrange<size_t>(DefaultCapacity)) {
        BOOST_CHECK(the_dict.empty_at(i));
    }
    // store value
    string the_key = "some key";
    hash_type the_hash = 15; // make things even more complicated, storing all keys with the same hash
    void * the_value = (void *) 42;
    BOOST_CHECK(the_dict.put(the_key, the_hash, the_value) == true);
    // store several values with the same hash
    BOOST_CHECK(the_dict.put("some key 1", the_hash, the_value) == true);
    BOOST_CHECK(the_dict.put("some key 2", the_hash, the_value) == true);
    BOOST_CHECK(the_dict.put("some sneaky key", the_hash + 1, the_value) == true);
    BOOST_CHECK(the_dict.put("some key 3", the_hash, the_value) == true);
    BOOST_CHECK(the_dict.put("some key 4", the_hash, the_value) == true);
    BOOST_CHECK(the_dict.put("some key 5", the_hash, the_value) == true);
    // check stored value
    bool found;
    table_type::mapped_type value;
    tie(found, value) = the_dict.get(the_key, the_hash);
    BOOST_CHECK(found && value == the_value);
    // check that now hash_table is non-empty
    BOOST_CHECK(!the_dict.empty());
    // attempt to add second value with the same key must return false
    BOOST_CHECK(the_dict.put(the_key, the_hash, the_value) == false);
    // delete stored value
    BOOST_CHECK(the_dict.del(the_key, the_hash) == true);
    // check that value was deleted
    tie(found, value) = the_dict.get(the_key, the_hash);
    BOOST_CHECK(!found);
    // attempt to delete non-existing value must return false
    BOOST_CHECK(the_dict.del(the_key, the_hash) == false);
    // unconditionally store value
    BOOST_CHECK(the_dict.put(the_key, the_hash, the_value) == true);
    // check stored value
    tie(found, value) = the_dict.get(the_key, the_hash);
    BOOST_CHECK(found && value == the_value);
    // unconditionally replace stored value
    void * some_other_value = (void *)7734;
    BOOST_CHECK(the_dict.put(the_key, the_hash, some_other_value) == false); // this time value must be replaced
    // check stored value
    tie(found, value) = the_dict.get(the_key, the_hash);
    BOOST_CHECK(found && value == some_other_value);
    // replace existing value
    BOOST_CHECK(the_dict.put(the_key, the_hash, the_value) == false);
    // ensure that value is truly replaced
    tie(found, value) = the_dict.get(the_key, the_hash);
    BOOST_CHECK(found && value == the_value);
    // delete stored value
    BOOST_CHECK(the_dict.del(the_key, the_hash) == true);
    BOOST_CHECK(the_dict.del(the_key, the_hash) == false);
    // check that all the dummy values are in
    BOOST_CHECK(the_dict.size() == 6);
    tie(found, value) = the_dict.get("some key 3", the_hash);
    BOOST_CHECK(found);
    tie(found, value) = the_dict.get("some key 5", the_hash);
    BOOST_CHECK(found);
    BOOST_CHECK(the_dict.del("some key 5", the_hash) == true);
    BOOST_CHECK(the_dict.del("some key 4", the_hash) == true);
    BOOST_CHECK(the_dict.del("some key 3", the_hash) == true);
    BOOST_CHECK(the_dict.del("some key 2", the_hash) == true);
    BOOST_CHECK(the_dict.del("some key 1", the_hash) == true);
    BOOST_CHECK(the_dict.del("some sneaky key", the_hash + 1) == true);
    BOOST_CHECK(the_dict.empty());
}


BOOST_AUTO_TEST_SUITE_END()

}


#include "unit_test.h"
#include <cachelot/stats.h>
#include <cachelot/cache.h>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_stats)

static auto calc_hash = fnv1a<cache::Cache::hash_type>::hasher();

cache::ItemPtr CreateItem(cache::Cache & c, const string k, const string v, cache::opaque_flags_type flags = 0, cache::seconds keepalive = cache::keepalive_forever ) {
    const auto key = bytes(k.c_str(), k.length());
    const auto value = bytes(v.c_str(), v.length());
    auto item = c.create_item(key, calc_hash(key), value.length(), flags, keepalive);
    item->assign_value(value);
    return item;
}

BOOST_AUTO_TEST_CASE(test_cache_commands) {
    cache::Cache the_cache(1024*1024*4, 1024*4, 16, false);
    const auto non_existing = bytes::from_literal("Non-existing key");
    // set
    {
        const auto item1 = CreateItem(the_cache, "Key1", "Valu1");
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_set), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_new), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_existing), 0);
        const auto item1_2 = CreateItem(the_cache, "Key1", "Valu2");
        BOOST_CHECK_EQUAL(the_cache.do_set(item1_2), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_set), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_new), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_existing), 1);
    }
    // get
    {
        BOOST_CHECK(the_cache.do_get(non_existing, calc_hash(non_existing)) == nullptr);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_get), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_misses), 1);
        const auto the_key = bytes::from_literal("Key1");
        BOOST_CHECK(the_cache.do_get(the_key, calc_hash(the_key)) != nullptr);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_get), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_misses), 1);
    }
    // add
    {
        const auto item1 = CreateItem(the_cache, "Add_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_add(item1), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_add), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_not_stored), 0);
        const auto item1_2 = CreateItem(the_cache, "Add_Key1", "Value2");
        BOOST_CHECK_EQUAL(the_cache.do_add(item1_2), cache::NOT_STORED);
        the_cache.destroy_item(item1_2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_add), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_not_stored), 1);
    }
    // replace
    {
        const auto item1 = CreateItem(the_cache, "Replace_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_replace(item1), cache::NOT_STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_replace), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_not_stored), 1);
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto item1_2 = CreateItem(the_cache, "Replace_Key1", "Value2");
        BOOST_CHECK_EQUAL(the_cache.do_replace(item1_2), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_replace), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_not_stored), 1);
    }
    // cas
    {
        const auto item1 = CreateItem(the_cache, "CAS_Key1", "Value1");
        const auto item1_timestamp = item1->timestamp();
        BOOST_CHECK_EQUAL(the_cache.do_cas(item1, 0), cache::NOT_FOUND);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 0);
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto item1_2 = CreateItem(the_cache, "CAS_Key1", "Value2");
        BOOST_CHECK_EQUAL(the_cache.do_cas(item1_2, item1_timestamp), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 0);
        const auto item1_3 = CreateItem(the_cache, "CAS_Key1", "Value3");
        BOOST_CHECK_EQUAL(the_cache.do_cas(item1_3, item1_timestamp), cache::EXISTS);
        the_cache.destroy_item(item1_3);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 3);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 1);
    }
    // delete
    {
        BOOST_CHECK(the_cache.do_delete(non_existing, calc_hash(non_existing)) == cache::NOT_FOUND);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_delete), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_misses), 1);
        const auto item1 = CreateItem(the_cache, "Delete_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto the_key = bytes::from_literal("Delete_Key1");
        BOOST_CHECK(the_cache.do_delete(the_key, calc_hash(the_key)) == cache::DELETED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_delete), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_misses), 1);
    }
    // touch
    {
        BOOST_CHECK(the_cache.do_touch(non_existing, calc_hash(non_existing), cache::keepalive_forever) == cache::NOT_FOUND);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_touch), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_misses), 1);
        const auto item1 = CreateItem(the_cache, "Touch_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto the_key = bytes::from_literal("Touch_Key1");
        BOOST_CHECK(the_cache.do_touch(the_key, calc_hash(the_key), cache::keepalive_forever) == cache::TOUCHED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_touch), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_misses), 1);
    }
    // incr / decr
    {
        the_cache.do_incr(non_existing, calc_hash(non_existing), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_incr), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_misses), 1);
        the_cache.do_decr(non_existing, calc_hash(non_existing), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_decr), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_misses), 1);
        const auto item1 = CreateItem(the_cache, "Arithmetic_Key1", "0");
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto the_key = bytes::from_literal("Arithmetic_Key1");
        the_cache.do_incr(the_key, calc_hash(the_key), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_incr), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_misses), 1);
        the_cache.do_decr(the_key, calc_hash(the_key), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_decr), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_misses), 1);
    }
    // append
    {
        const auto item1 = CreateItem(the_cache, "Append_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_append(item1), cache::NOT_FOUND);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_append), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_misses), 1);
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto item1_1 = CreateItem(the_cache, "Append_Key1", "Value2");
        BOOST_CHECK_EQUAL(the_cache.do_append(item1_1), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_append), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_misses), 1);
    }
    // prepend
    {
        const auto item1 = CreateItem(the_cache, "Prepend_Key1", "Value1");
        BOOST_CHECK_EQUAL(the_cache.do_prepend(item1), cache::NOT_FOUND);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_prepend), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_misses), 1);
        BOOST_CHECK_EQUAL(the_cache.do_set(item1), cache::STORED);
        const auto item1_1 = CreateItem(the_cache, "Prepend_Key1", "Value2");
        BOOST_CHECK_EQUAL(the_cache.do_prepend(item1_1), cache::STORED);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_prepend), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_misses), 1);
    }
    // flush_all
    {
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_flush), 0);
        the_cache.do_flush_all();
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_flush), 1);
    }
}

BOOST_AUTO_TEST_CASE(test_cache_size) {
    cache::Cache the_cache(1024*1024*4, 1024*4, 16, false);
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 16);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 0);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
    std::vector<string> keys;
    for (unsigned i = 0; i < 16; ++i) {
        auto k = random_string(10, 15);
        keys.push_back(k);
        const auto item = CreateItem(the_cache, k, random_string(1, 30));
        BOOST_CHECK_EQUAL(the_cache.do_add(item), cache::STORED);
    }
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 32);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 16);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
    for (auto k : keys) {
        auto key = bytes(k.c_str(), k.length());
        BOOST_CHECK_EQUAL(the_cache.do_delete(key, calc_hash(key)), cache::DELETED);
    }
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 32);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 0);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
}


BOOST_AUTO_TEST_SUITE_END()

} //anonymous namespace


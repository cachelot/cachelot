#include "unit_test.h"
#include <cachelot/stats.h>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_stats)

BOOST_AUTO_TEST_CASE(test_no_overflow_arith) {
    return;
    // unsigned
    static constexpr auto size_t_max = std::numeric_limits<size_t>::max();
    static constexpr auto uint32_max = std::numeric_limits<uint32>::max();
    // increment
    BOOST_CHECK_EQUAL(no_overflow_increment<uint32>(1u, 1), 2u);
    BOOST_CHECK_EQUAL(no_overflow_increment<uint32>(1u, 0), 1u);
    BOOST_CHECK_EQUAL(no_overflow_increment<uint32>(uint32_max - 1, 1), uint32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<uint32>(uint32_max, 1), uint32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<uint32>(0, size_t_max), uint32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<size_t>(0, size_t_max), size_t_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<size_t>(1, size_t_max), size_t_max);
    // decrement
    BOOST_CHECK_EQUAL(no_overflow_decrement<uint32>(1u, 0), 1u);
    BOOST_CHECK_EQUAL(no_overflow_decrement<uint32>(1u, 1u), 0);
    BOOST_CHECK_EQUAL(no_overflow_decrement<uint32>(0, 1u), 0);
    BOOST_CHECK_EQUAL(no_overflow_decrement<uint32>(0, std::numeric_limits<size_t>::max()), 0);
    BOOST_CHECK_EQUAL(no_overflow_decrement<size_t>(size_t_max, size_t_max), 0);

    // signed
    static constexpr auto int32_max = std::numeric_limits<int32>::max();
    static constexpr auto int32_min = std::numeric_limits<int32>::min();
    // increment
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(1, 1), 2);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(1, 0), 1);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(int32_max - 1, 1), int32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(int32_max, 1), int32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(0, (size_t)int32_max), int32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(0, size_t_max), int32_max);
    BOOST_CHECK_EQUAL(no_overflow_increment<int32>(int32_max, size_t_max), int32_max);
    // decrement
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(1u, 0), 1u);
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(1u, 1u), 0);
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(0, 1u), -1);
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(int32_min+1, 1u), int32_min);
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(int32_min, 1u), int32_min);
    BOOST_CHECK_EQUAL(no_overflow_decrement<int32>(int32_min, size_t_max), int32_min);
}

BOOST_AUTO_TEST_SUITE_END()

} //anonymous namespace


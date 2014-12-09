#include "unit_test.h"
#include <cachelot/bits.h>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_bits)

BOOST_AUTO_TEST_CASE(test_pow2_utils) {

    BOOST_CHECK(ispow2(0u) == false);
    BOOST_CHECK(ispow2(1u) == true);
    BOOST_CHECK(ispow2(2u) == true);
    BOOST_CHECK(ispow2(1024u) == true);
    BOOST_CHECK(ispow2(1023u) == false);
    BOOST_CHECK(ispow2((size_t)std::pow(2,32)) == true);

    BOOST_CHECK_EQUAL(pow2(0u), (size_t)std::pow(2,0));
    BOOST_CHECK_EQUAL(pow2(1u), (size_t)std::pow(2,1));
    BOOST_CHECK_EQUAL(pow2(2u), (size_t)std::pow(2,2));
    BOOST_CHECK_EQUAL(pow2(32ull), (uint64)std::pow(2,32));
    BOOST_CHECK_EQUAL(pow2(63ull), (uint64)std::pow(2,63));

    BOOST_CHECK_EQUAL(log2u(2u), 1u);
    BOOST_CHECK_EQUAL(log2u(1u), 0);
    BOOST_CHECK_EQUAL(log2u(pow2(63ull)), 63);
    BOOST_CHECK_EQUAL(log2u(32u), 5);
    BOOST_CHECK_EQUAL(log2u(0x80000000ull), 31);
    BOOST_CHECK_EQUAL(log2u(0x10000000000ull), 40);
}

BOOST_AUTO_TEST_CASE(test_bit_basic) {
    uint32 i = 0;
    BOOST_CHECK(bit::isset(i, 0) == false);
    BOOST_CHECK(bit::isset(1, 0) == true);
    i = bit::set(i, 0);
    BOOST_CHECK(i == 1);
    i = bit::unset(i, 0);
    i = bit::set(i, 31);
    BOOST_CHECK(i == 0x80000000);
    BOOST_CHECK(bit::most_significant(i) == 31);
    BOOST_CHECK(bit::isset(i, 31) == true);
    i = bit::flip(i, 31);
    BOOST_CHECK(i == 0);
}

BOOST_AUTO_TEST_SUITE_END()

}

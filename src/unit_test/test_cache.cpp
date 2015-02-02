#include "unit_test.h"
#include <cachelot/cache.h>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_cache)


BOOST_AUTO_TEST_CASE(test_cache_basic) {
    BOOST_CHECK_EQUAL(2, 2)
}


BOOST_AUTO_TEST_CASE(test_expiration) {
    BOOST_CHECK_EQUAL(2, 2)
}


BOOST_AUTO_TEST_SUITE_END()

}

#include "unit_test.h"
#include <cachelot/io_buffer.h>
#include <cstdlib>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_io_buffer)

BOOST_AUTO_TEST_CASE(test_io_buffer_basic) {
    io_buffer buf(0, 16);
    BOOST_CHECK_EQUAL(0, buf.capacity());
    static const char pattern[] = "Test string";
    char * ptr = buf.begin_write(sizeof(pattern));
    std::sprintf(ptr, pattern);
    buf.complete_write(sizeof(pattern));
    BOOST_CHECK_EQUAL(buf.non_read(), sizeof(pattern));
    bytes read = buf.try_read_until(bytes::from_literal("\0"));
    BOOST_CHECK_EQUAL(read.length(), sizeof(pattern));
    BOOST_CHECK(std::memcmp(read.begin(), pattern, sizeof(pattern) - 1) == 0);
    BOOST_CHECK_THROW(buf.begin_write(17), std::length_error);
    buf.discard_all();
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
    ptr = buf.begin_write(16);
    std::memset(ptr, 'X', 16);
    buf.complete_write(16);
    read = buf.try_read_until(bytes::from_literal("XXXXXXXXXXXXXXXX"));
    BOOST_CHECK_EQUAL(read.length(), 16);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


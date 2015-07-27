#include "unit_test.h"
#include <server/io_buffer.h>
#include <cstdlib>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_io_buffer)

BOOST_AUTO_TEST_CASE(test_io_buffer_basic) {
    io_buffer buf(0, 16);
    BOOST_CHECK_EQUAL(0, buf.capacity());
    static const char pattern[] = "Test string";
    // write test pattern
    char * ptr = buf.begin_write(sizeof(pattern));
    std::sprintf(ptr, pattern);
    buf.confirm_write(sizeof(pattern));
    BOOST_CHECK_EQUAL(buf.non_read(), sizeof(pattern));
    // read from buffer
    bytes read = buf.try_read_until(bytes::from_literal("\0"));
    BOOST_CHECK_EQUAL(read.length(), sizeof(pattern));
    BOOST_CHECK(std::memcmp(read.begin(), pattern, sizeof(pattern) - 1) == 0);
    BOOST_CHECK_THROW(buf.begin_write(17), std::length_error);
    // reset buffer
    buf.reset();
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
    // write 16 bytes of nothing and discard it immediately
    auto w_savepoint = buf.write_savepoint();
    ptr = buf.begin_write(16);
    buf.confirm_write(16);
    buf.discard_written(w_savepoint);
    // write 16 bytes of 'X'
    ptr = buf.begin_write(16);
    std::memset(ptr, 'X', 16);
    buf.confirm_write(16);
    // read and check
    read = buf.try_read_until(bytes::from_literal("XXXXXXXXXXXXXXXX"));
    BOOST_CHECK_EQUAL(read.length(), 16);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


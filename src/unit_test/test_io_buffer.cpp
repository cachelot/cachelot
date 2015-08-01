#include "unit_test.h"
#include <server/io_buffer.h>
#include <cstdlib>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_io_buffer)

BOOST_AUTO_TEST_CASE(test_io_buffer_capacity) {
    io_buffer buf(0, 16);
    // test capacity
    BOOST_CHECK_EQUAL(0, buf.capacity());
    BOOST_CHECK_THROW(buf.begin_write(17), std::length_error);
    // test reset buffer
    buf.reset();
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
}

BOOST_AUTO_TEST_CASE(test_io_buffer_read_write) {
    io_buffer buf(0, 64);
    static const char pattern[] = "Test string [separator] more [separator]";
    // test write
    std::memcpy(buf.begin_write(sizeof(pattern)), pattern, std::strlen(pattern));
    buf.confirm_write(std::strlen(pattern));
    // test read
    BOOST_CHECK_EQUAL(buf.non_read(), std::strlen(pattern));
    const auto Test = buf.begin_read();
    buf.confirm_read(std::strlen("Test"));
    BOOST_CHECK(std::strncmp(Test, "Test", std::strlen("Test")) == 0);
    // test try_read_until
    auto read1 = buf.try_read_until(bytes::from_literal("[separator]"));
    BOOST_CHECK_EQUAL(string(read1.begin(), read1.length()), string(" string [separator]"));
    BOOST_CHECK_EQUAL(buf.non_read(), std::strlen(" more [separator]"));
    auto read2 = buf.try_read_until(bytes::from_literal("[separator]"));
    BOOST_CHECK_EQUAL(string(read2.begin(), read2.length()), string(" more [separator]"));
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
}

BOOST_AUTO_TEST_CASE(test_io_buffer_savepoint) {
    io_buffer buf(0, 16);
    // write 16 bytes of nothing and discard it immediately
    auto w_savepoint = buf.begin_write_transaction();
    buf.begin_write(16);
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
    buf.confirm_write(16);
    BOOST_CHECK_EQUAL(buf.non_read(), 16);
    buf.rollback_write_transaction(w_savepoint);
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
    // test read savepoint
    buf.reset();
    buf.begin_write(16);
    BOOST_CHECK_EQUAL(buf.non_read(), 0);
    buf.confirm_write(16);
    BOOST_CHECK_EQUAL(buf.non_read(), 16);
    auto r_savepoint = buf.begin_read_transaction();
    buf.begin_read();
    buf.confirm_read(1);
    BOOST_CHECK_EQUAL(buf.non_read(), 15);
    buf.rollback_read_transaction(r_savepoint);
    BOOST_CHECK_EQUAL(buf.non_read(), 16);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


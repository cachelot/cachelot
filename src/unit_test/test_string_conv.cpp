#include "unit_test.h"
#include <cachelot/string_conv.h>
#include <sstream>
#include <cstring>

namespace {

using namespace cachelot;

#define CHECK_SYSTEM_ERROR(expr, error) \
    try { \
        expr; \
        BOOST_CHECK(false && "Exception expected"); \
    } catch (const system_error & exc) { \
        BOOST_CHECK_EQUAL(exc.code(), error); \
    }

BOOST_AUTO_TEST_SUITE(test_string_conv)

template <typename NumType>
void check_int_to_string(NumType number) {
    std::stringstream stream;
    stream << number;
    string their_value = stream.str();
    constexpr size_t MAXLEN = 32;
    char my_value[MAXLEN];
    size_t my_value_length = int_to_str<NumType, char *>(number, my_value);
    BOOST_ASSERT(my_value_length > 0);
    BOOST_ASSERT(my_value_length < MAXLEN);
    my_value[my_value_length] = '\0';
    BOOST_CHECK_EQUAL(my_value, their_value);
}


BOOST_AUTO_TEST_CASE(test_int_to_str) {
    check_int_to_string(0);
    check_int_to_string(1);
    check_int_to_string(-1);
    check_int_to_string(std::numeric_limits<uint64>::max());
    check_int_to_string(std::numeric_limits<int64>::min());
    check_int_to_string(std::numeric_limits<int64>::max());
    check_int_to_string(std::numeric_limits<uint32>::max());
    check_int_to_string(std::numeric_limits<int32>::min());
    check_int_to_string(std::numeric_limits<int32>::max());
}

BOOST_AUTO_TEST_CASE(test_num_ascii_length) {
    BOOST_CHECK_EQUAL(int_ascii_length(0), 1);
    BOOST_CHECK_EQUAL(int_ascii_length(-1234567890), 11);
    BOOST_CHECK_EQUAL(uint_ascii_length(0u), 1);
    BOOST_CHECK_EQUAL(uint_ascii_length(1234567890u), 10);
}

template <typename NumType>
string num_to_str(NumType number) {
    std::stringstream stream;
    stream << number;
    return stream.str();
}

BOOST_AUTO_TEST_CASE(test_str_to_int) {
    // int 32-bit
    int inum = 0;
    string s = num_to_str(inum);
    BOOST_CHECK_EQUAL(str_to_int<int>(s.begin(), s.end()), inum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned int>(s.begin(), s.end()), inum);
    inum = std::numeric_limits<int>::max();
    s = num_to_str(inum);
    BOOST_CHECK_EQUAL(str_to_int<int>(s.begin(), s.end()), inum);
    inum = std::numeric_limits<int>::min();
    s = num_to_str(inum);
    BOOST_CHECK_EQUAL(str_to_int<int>(s.begin(), s.end()), inum);
    inum = -3;
    s = num_to_str(inum);
    BOOST_CHECK_EQUAL(str_to_int<int>(s.begin(), s.end()), inum);
    auto uinum = std::numeric_limits<unsigned int>::max();
    s = num_to_str(uinum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned int>(s.begin(), s.end()), uinum);
    s = "-0";
    BOOST_CHECK_EQUAL(str_to_int<int>(s.begin(), s.end()), 0);

    // signed int 64-bit
    auto llnum = std::numeric_limits<long long>::max();
    s = num_to_str(llnum);
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), llnum);
    llnum = std::numeric_limits<long long>::min();
    s = num_to_str(llnum);
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), llnum);
    llnum = std::numeric_limits<long long>::min() + 1;
    s = num_to_str(llnum);
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), llnum);
    llnum = std::numeric_limits<long long>::max() - 1;
    s = num_to_str(llnum);
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), llnum);
    s = "00000000000";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), 0);
    s = "-00000000000";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), 0);
    s = "-000000000001";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), -1);
    s = "0000000000010";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), 10);
    s = "10000000001";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), 10000000001);
    s = "-0";
    BOOST_CHECK_EQUAL(str_to_int<long long>(s.begin(), s.end()), 0);

    // unsigned int 64-bit
    auto ullnum = std::numeric_limits<unsigned long long>::max();
    s = num_to_str(ullnum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned long long>(s.begin(), s.end()), ullnum);
    ullnum = std::numeric_limits<unsigned long long>::max() - 1;
    s = num_to_str(ullnum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned long long>(s.begin(), s.end()), ullnum);

    // overflow errors
    s = num_to_str<unsigned long long>(std::numeric_limits<unsigned long long>::max());
    CHECK_SYSTEM_ERROR(str_to_int<long>(s.begin(), s.end()), error::numeric_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.begin(), s.end()), error::numeric_overflow);
    s = "2837468273468273468273468276348276348617623571564236714523";
    CHECK_SYSTEM_ERROR(str_to_int<long long>(s.begin(), s.end()), error::numeric_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.begin(), s.end()), error::numeric_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<char>(s.begin(), s.end()), error::numeric_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned char>(s.begin(), s.end()), error::numeric_overflow);
    s = "18446744073709551616"; // max + 1
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.begin(), s.end()), error::numeric_overflow);
    s = "28446744073709551615"; // max + 10000000000000000000
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.begin(), s.end()), error::numeric_overflow);
    s = num_to_str<long long>(static_cast<long long>(std::numeric_limits<int>::max()) + 1);
    CHECK_SYSTEM_ERROR(str_to_int<int>(s.begin(), s.end()), error::numeric_overflow);
    s = num_to_str<long long>(static_cast<long long>(std::numeric_limits<int>::min()) - 1);
    CHECK_SYSTEM_ERROR(str_to_int<int>(s.begin(), s.end()), error::numeric_overflow);
    s = "-9223372036854775809"; // min - 1
    CHECK_SYSTEM_ERROR(str_to_int<long long>(s.begin(), s.end()), error::numeric_overflow);
    s = "9223372036854775808"; // max + 1
    CHECK_SYSTEM_ERROR(str_to_int<long long>(s.begin(), s.end()), error::numeric_overflow);

    // convertion errors
    s = "00Nan";
    CHECK_SYSTEM_ERROR(str_to_int<int>(s.begin(), s.end()), error::numeric_convert);
    s = "-1";
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.begin(), s.end()), error::numeric_convert);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.begin(), s.end()), error::numeric_convert);
    s = "-";
    CHECK_SYSTEM_ERROR(str_to_int<int>(s.begin(), s.end()), error::numeric_convert);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.begin(), s.end()), error::numeric_convert);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


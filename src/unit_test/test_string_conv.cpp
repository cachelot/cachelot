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

template <typename NumType>
string num_to_str(NumType number) {
    std::stringstream stream;
    stream << number;
    return stream.str();
}

BOOST_AUTO_TEST_CASE(test_str_to_int) {
    long lnum = 0;
    string s = num_to_str(lnum);
    BOOST_CHECK_EQUAL(str_to_int<long>(s.c_str(), s.size()), lnum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned long>(s.c_str(), s.size()), lnum);
    lnum = std::numeric_limits<long>::max();
    s = num_to_str(lnum);
    BOOST_CHECK_EQUAL(str_to_int<long>(s.c_str(), s.size()), lnum);
    lnum = std::numeric_limits<long>::min();
    s = num_to_str(lnum);
    BOOST_CHECK_EQUAL(str_to_int<long>(s.c_str(), s.size()), lnum);
    lnum = -3;
    s = num_to_str(lnum);
    BOOST_CHECK_EQUAL(str_to_int<long>(s.c_str(), s.size()), lnum);
    unsigned long ulnum = std::numeric_limits<unsigned long>::max();
    s = num_to_str(ulnum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned long>(s.c_str(), s.size()), ulnum);
    unsigned long long ullnum = std::numeric_limits<unsigned long long>::max();
    s = num_to_str(ullnum);
    BOOST_CHECK_EQUAL(str_to_int<unsigned long long>(s.c_str(), s.size()), ullnum);
    // check failure scenario
    s = num_to_str<unsigned long long>(std::numeric_limits<unsigned long long>::max());
    CHECK_SYSTEM_ERROR(str_to_int<long>(s.c_str(), s.size()), error::number_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.c_str(), s.size()), error::number_overflow);
    s = "2837468273468273468273468276348276348617623571564236714523";
    CHECK_SYSTEM_ERROR(str_to_int<long long>(s.c_str(), s.size()), error::number_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.c_str(), s.size()), error::number_overflow);
    s = "Nan";
    CHECK_SYSTEM_ERROR(str_to_int<int>(s.c_str(), s.size()), error::invalid_argument);
    s = "-1";
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.c_str(), s.size()), error::number_overflow);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.c_str(), s.size()), error::number_overflow);
    s = "-";
    CHECK_SYSTEM_ERROR(str_to_int<unsigned int>(s.c_str(), s.size()), error::invalid_argument);
    CHECK_SYSTEM_ERROR(str_to_int<unsigned long long>(s.c_str(), s.size()), error::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymouse namespace


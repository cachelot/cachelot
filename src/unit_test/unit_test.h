#ifndef CACHELOT_UNIT_TEST_H_INCLUDED
#define CACHELOT_UNIT_TEST_H_INCLUDED

#include <cachelot/cachelot.h>

#if defined(__GNUC__)
// suppress warnings from the following headers
#  pragma GCC system_header
#endif

// disable auto linking (#pragma lib)
#define BOOST_TEST_NO_LIB
// Enable messages from unit tests
#define BOOST_TEST_LOG_LEVEL boost::unit_test::log_messages
#include <boost/test/unit_test.hpp>
#include <boost/test/debug.hpp>

namespace cachelot {

    /// generate random string of `length` chars
    inline string random_string(size_t length) {
        auto randchar = []() -> char {
            static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
            const size_t max_index = (sizeof(charset) - 1);
            return charset[ rand() % max_index ];
        };
        string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    }

} // namespace cachelot

#endif // CACHELOT_UNIT_TEST_H_INCLUDED

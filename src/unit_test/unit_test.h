#ifndef CACHELOT_UNIT_TEST_H_INCLUDED
#define CACHELOT_UNIT_TEST_H_INCLUDED

#include <cachelot/cachelot.h>

#if defined(__GNUC__)
// suppress warnings from the following headers
#  pragma GCC system_header
#endif

#include <random>
// disable auto linking (#pragma lib)
#define BOOST_TEST_NO_LIB
// Enable messages from unit tests
#define BOOST_TEST_LOG_LEVEL boost::unit_test::log_messages
#include <boost/test/unit_test.hpp>
#include <boost/test/debug.hpp>

namespace cachelot {

    /// generate random integer in [min, max] range
    template <typename IntType = int>
    class random_int {
    public:
        explicit random_int(IntType minval, IntType maxval)
            : m_rnd_engine()
            , m_rnd_gen(minval, maxval) {
        }

        random_int(const random_int &) = default;
        random_int & operator= (const random_int &) = default;

        IntType operator() () {
            return m_rnd_gen(m_rnd_engine);
        }
    private:
        std::random_device m_rnd_engine;
        std::uniform_int_distribution<IntType> m_rnd_gen;
    };

    /// generate random string of `length` chars
    inline string random_string(size_t length) {
        static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        random_int<size_t> rnd_gen(0, sizeof(charset) - 1);
        auto randchar = [&rnd_gen]() -> char {
            return charset[ rnd_gen() ];
        };
        string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    }

} // namespace cachelot

#endif // CACHELOT_UNIT_TEST_H_INCLUDED

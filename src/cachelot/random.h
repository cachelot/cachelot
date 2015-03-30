#ifndef CACHELOT_RANDOM_H_INCLUDED
#define CACHELOT_RANDOM_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <random>

/// @ingroup common
/// @{

namespace cachelot {

    /// generate random integer in [min, max] range
    template <typename IntType = int>
    class random_int {
    public:
        explicit random_int(IntType minval, IntType maxval)
            : m_rnd_gen(minval, maxval) {
        }

        random_int(const random_int &) = default;
        random_int & operator= (const random_int &) = default;

        IntType operator() () {
            return m_rnd_gen(random_engine);
        }
    private:
        typedef std::minstd_rand random_engine_type;
        static random_engine_type random_engine;
        std::uniform_int_distribution<IntType> m_rnd_gen;
    };

    template <typename IntType>
    typename random_int<IntType>::random_engine_type random_int<IntType>::random_engine;

    /// generate random string of `length` chars from pre-defined alphabet
    inline string random_string(size_t minlen, size_t maxlen) {
        static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        static random_int<size_t> rnd_gen(0, sizeof(charset) - 1);
        auto randchar = [=]() -> char {
            return charset[ rnd_gen() ];
        };
        static random_int<size_t> rnd_length(minlen, maxlen);
        auto const length = rnd_length();
        string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    }

} // namespace cachelot

/// @}

#endif // CACHELOT_RANDOM_H_INCLUDED

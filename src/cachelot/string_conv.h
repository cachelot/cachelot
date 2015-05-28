#ifndef CACHELOT_STRING_CONV_H_INCLUDED
#define CACHELOT_STRING_CONV_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif


/// @ingroup common
/// @{

namespace cachelot {

    /// @defgroup conv Type to string and vice versa conversion
    /// @{

    namespace internal {
        template <typename IntType> struct numeric;
        template <> struct numeric<int8> { typedef uint8 unsigned_; static constexpr size_t max_str_length = 4; };
        template <> struct numeric<int16> { typedef uint16 unsigned_; static constexpr size_t max_str_length = 6; };
        template <> struct numeric<int32> { typedef uint32 unsigned_; static constexpr size_t max_str_length = 11; };
        template <> struct numeric<int64> { typedef uint64 unsigned_; static constexpr size_t max_str_length = 20; };
        template <> struct numeric<uint8> { typedef uint8 unsigned_; static constexpr size_t max_str_length = 3; };
        template <> struct numeric<uint16> { typedef uint16 unsigned_; static constexpr size_t max_str_length = 5; };
        template <> struct numeric<uint32> { typedef uint32 unsigned_; static constexpr size_t max_str_length = 10; };
        template <> struct numeric<uint64> { typedef uint64 unsigned_; static constexpr size_t max_str_length = 20; };

        template <typename IntType, bool Signed = std::is_signed<IntType>::value> struct is_negative;
        template <typename IntType> struct is_negative<IntType, false> {
            static constexpr bool value(IntType) { return false; };
        };
        template <typename IntType> struct is_negative<IntType, true> {
            static constexpr bool value(IntType v) { return v < 0; };
        };


        class int_to_str_impl {
            // Integer to String convertion
            //
            // following implementation based on String Toolkit Library by Arash Partow
            // http://www.partow.net/programming/strtk/index.html
            // String Toolkit Library distributed under terms of Common Public License
            // http://opensource.org/licenses/cpl1.0.php
            static char _3digit_table(size_t index) noexcept {
                static const char table[] = {
                    "000001002003004005006007008009010011012013014015016017018019020021022023024"
                    "025026027028029030031032033034035036037038039040041042043044045046047048049"
                    "050051052053054055056057058059060061062063064065066067068069070071072073074"
                    "075076077078079080081082083084085086087088089090091092093094095096097098099"
                    "100101102103104105106107108109110111112113114115116117118119120121122123124"
                    "125126127128129130131132133134135136137138139140141142143144145146147148149"
                    "150151152153154155156157158159160161162163164165166167168169170171172173174"
                    "175176177178179180181182183184185186187188189190191192193194195196197198199"
                    "200201202203204205206207208209210211212213214215216217218219220221222223224"
                    "225226227228229230231232233234235236237238239240241242243244245246247248249"
                    "250251252253254255256257258259260261262263264265266267268269270271272273274"
                    "275276277278279280281282283284285286287288289290291292293294295296297298299"
                    "300301302303304305306307308309310311312313314315316317318319320321322323324"
                    "325326327328329330331332333334335336337338339340341342343344345346347348349"
                    "350351352353354355356357358359360361362363364365366367368369370371372373374"
                    "375376377378379380381382383384385386387388389390391392393394395396397398399"
                    "400401402403404405406407408409410411412413414415416417418419420421422423424"
                    "425426427428429430431432433434435436437438439440441442443444445446447448449"
                    "450451452453454455456457458459460461462463464465466467468469470471472473474"
                    "475476477478479480481482483484485486487488489490491492493494495496497498499"
                    "500501502503504505506507508509510511512513514515516517518519520521522523524"
                    "525526527528529530531532533534535536537538539540541542543544545546547548549"
                    "550551552553554555556557558559560561562563564565566567568569570571572573574"
                    "575576577578579580581582583584585586587588589590591592593594595596597598599"
                    "600601602603604605606607608609610611612613614615616617618619620621622623624"
                    "625626627628629630631632633634635636637638639640641642643644645646647648649"
                    "650651652653654655656657658659660661662663664665666667668669670671672673674"
                    "675676677678679680681682683684685686687688689690691692693694695696697698699"
                    "700701702703704705706707708709710711712713714715716717718719720721722723724"
                    "725726727728729730731732733734735736737738739740741742743744745746747748749"
                    "750751752753754755756757758759760761762763764765766767768769770771772773774"
                    "775776777778779780781782783784785786787788789790791792793794795796797798799"
                    "800801802803804805806807808809810811812813814815816817818819820821822823824"
                    "825826827828829830831832833834835836837838839840841842843844845846847848849"
                    "850851852853854855856857858859860861862863864865866867868869870871872873874"
                    "875876877878879880881882883884885886887888889890891892893894895896897898899"
                    "900901902903904905906907908909910911912913914915916917918919920921922923924"
                    "925926927928929930931932933934935936937938939940941942943944945946947948949"
                    "950951952953954955956957958959960961962963964965966967968969970971972973974"
                    "975976977978979980981982983984985986987988989990991992993994995996997998999"
                    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                };

                debug_assert(index < sizeof(table));
                return table[index];
            }

            static char _2digit_table(size_t index) noexcept {
                static const char table[] = {
                    "0001020304050607080910111213141516171819"
                    "2021222324252627282930313233343536373839"
                    "4041424344454647484950515253545556575859"
                    "6061626364656667686970717273747576777879"
                    "8081828384858687888990919293949596979899"
                    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                };
                debug_assert(index < sizeof(table));
                return table[index];
            }

            static const size_t radix = 10;
            static const size_t radix_sqr = radix * radix;
            static const size_t radix_cube = radix * radix * radix;
            static const size_t buffer_size = 24;
        public:
            template <typename IntType, typename OutputIterator>
            static size_t convert(const IntType value, OutputIterator dest) noexcept {
                char buffer[buffer_size];

                if (value == 0) {
                    *dest = '0';
                    return 1;
                }

                char * itr = buffer + buffer_size;
                char const * buffer_end = itr;
                const bool negative = is_negative<IntType>::value(value);

                typedef typename numeric<IntType>::unsigned_ UnsignedIntType;
                UnsignedIntType xvalue;
                if (negative) {
                    *dest++ = '-';
                    xvalue = - value;
                } else {
                    xvalue = value;
                }

                while (xvalue >= static_cast<UnsignedIntType>(radix_sqr)) {
                    UnsignedIntType temp_v = xvalue / radix_cube;
                    const size_t pos_in_table = 3 * (xvalue - (temp_v * radix_cube));
                    *(--itr) = _3digit_table(pos_in_table + 2);
                    *(--itr) = _3digit_table(pos_in_table + 1);
                    *(--itr) = _3digit_table(pos_in_table + 0);
                    xvalue = temp_v;
                }

                while (xvalue >= static_cast<UnsignedIntType>(radix)) {
                    UnsignedIntType temp_v = xvalue / radix_sqr;
                    const size_t pos_in_table = 2 * (xvalue - (temp_v * radix_sqr));
                    *(--itr) = _2digit_table(pos_in_table + 1);
                    *(--itr) = _2digit_table(pos_in_table + 0);
                    xvalue = temp_v;
                }

                if (xvalue > 0) {
                    *(--itr) = static_cast<unsigned char>('0' + xvalue);
                }
                const size_t length = buffer_end - itr + static_cast<size_t>(negative);
                debug_assert(length <= buffer_size);
                while (itr < buffer_end) {
                    *dest++ = *itr++;
                }
                return length;
            }
        };

    } // namespace internal


    /// Covert ineger value to string and write it into provided output iterator
    /// @note caller is responsible for output buffer and its size
    template <typename IntType, typename OutputIterator>
    inline size_t int_to_str(IntType number, OutputIterator dest) noexcept {
        static_assert(std::is_integral<IntType>::value, "Non-integer type");
        return internal::int_to_str_impl::convert<IntType, OutputIterator>(number, dest);
    }


    /// Determine length of ASCII string number representation
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    inline size_t uint_ascii_length(UIntT number) noexcept {
        if (number == 0) {
            return 1;
        }
        return static_cast<size_t>(log10(number)) + 1;
    }

    template <typename IntT,
              class = typename std::enable_if<std::is_signed<IntT>::value>::type>
    inline size_t int_ascii_length(IntT number) noexcept {
        const size_t minus_sign_len = (number < 0) ? 1 : 0;
        auto unsigned_number = static_cast<typename internal::numeric<IntT>::unsigned_>(abs(number));
        return minus_sign_len + uint_ascii_length(unsigned_number);
    }

    namespace internal {

        // Compile-time recursive class to unroll the loop
        template <class ConstIterator, size_t NumUncoverted>
        struct str_to_big_unsigned_impl {
            static constexpr uint64 convert(uint64 current, ConstIterator iter, size_t length) noexcept {
                return str_to_big_unsigned_impl<ConstIterator, NumUncoverted - 1>::convert(current * 10 + (*iter - '0'), iter + 1, length);
            }
        };

        // Partial specialization terminates recursion
        template <class ConstIterator>
        struct str_to_big_unsigned_impl<ConstIterator, 1> {
            static constexpr uint64 convert(uint64 current, ConstIterator iter, size_t /*length*/) noexcept {
                return current * 10 + (*iter - '0');
            }
        };


        // Convert ASCII encoded decimal string to unsigned 64-bit integer
        template <class ConstIterator>
        inline uint64 str_to_big_unsigned(ConstIterator str, ConstIterator end, error_code & out_error) noexcept {
            // returned on error
            static constexpr uint64 NO_RESULT = std::numeric_limits<uint64>::max();

            const auto length = end - str;

            // shortcut
            if (length == 1 && *str == '0') {
                return 0u;
            }

            // check that all chars are digits
            for (auto iter = str; iter < end; ++iter) {
                if (not std::isdigit(*iter)) {
                    out_error = error::numeric_convert;
                    return NO_RESULT;
                }
            }

            switch (length) {
                case 20: // in ASCII max uint64 number length is 20
                    // check against the biggest uint64 number - 18446744073709551615
                    if (str[ 0] > '1' ||
                        str[ 1] > '8' ||
                        str[ 2] > '4' ||
                        str[ 3] > '4' ||
                        str[ 4] > '6' ||
                        str[ 5] > '7' ||
                        str[ 6] > '4' ||
                        str[ 7] > '4' ||
                        str[ 8] > '0' ||
                        str[ 9] > '7' ||
                        str[10] > '3' ||
                        str[11] > '7' ||
                        str[12] > '0' ||
                        str[13] > '9' ||
                        str[14] > '5' ||
                        str[15] > '5' ||
                        str[16] > '1' ||
                        str[17] > '6' ||
                        str[18] > '1' ||
                        str[19] > '5' ) {
                        out_error = error::numeric_overflow;
                        return NO_RESULT;
                    }
                    return str_to_big_unsigned_impl<ConstIterator, 20>::convert(0, str, length);
                case 19:
                    return str_to_big_unsigned_impl<ConstIterator, 19>::convert(0, str, length);
                case 18:
                    return str_to_big_unsigned_impl<ConstIterator, 18>::convert(0, str, length);
                case 17:
                    return str_to_big_unsigned_impl<ConstIterator, 17>::convert(0, str, length);
                case 16:
                    return str_to_big_unsigned_impl<ConstIterator, 16>::convert(0, str, length);
                case 15:
                    return str_to_big_unsigned_impl<ConstIterator, 15>::convert(0, str, length);
                case 14:
                    return str_to_big_unsigned_impl<ConstIterator, 14>::convert(0, str, length);
                case 13:
                    return str_to_big_unsigned_impl<ConstIterator, 13>::convert(0, str, length);
                case 12:
                    return str_to_big_unsigned_impl<ConstIterator, 12>::convert(0, str, length);
                case 11:
                    return str_to_big_unsigned_impl<ConstIterator, 11>::convert(0, str, length);
                case 10:
                    return str_to_big_unsigned_impl<ConstIterator, 10>::convert(0, str, length);
                case 9:
                    return str_to_big_unsigned_impl<ConstIterator, 9>::convert(0, str, length);
                case 8:
                    return str_to_big_unsigned_impl<ConstIterator, 8>::convert(0, str, length);
                case 7:
                    return str_to_big_unsigned_impl<ConstIterator, 7>::convert(0, str, length);
                case 6:
                    return str_to_big_unsigned_impl<ConstIterator, 6>::convert(0, str, length);
                case 5:
                    return str_to_big_unsigned_impl<ConstIterator, 5>::convert(0, str, length);
                case 4:
                    return str_to_big_unsigned_impl<ConstIterator, 4>::convert(0, str, length);
                case 3:
                    return str_to_big_unsigned_impl<ConstIterator, 3>::convert(0, str, length);
                case 2:
                    return str_to_big_unsigned_impl<ConstIterator, 2>::convert(0, str, length);
                case 1:
                    return str_to_big_unsigned_impl<ConstIterator, 1>::convert(0, str, length);
                case 0:
                    out_error = error::numeric_convert;
                    return NO_RESULT;
                default:
                    out_error = error::numeric_overflow;
                    return NO_RESULT;
            }
        }

        // Convert ASCII encoded decimal string to unsigned 64-bit integer
        template <class ConstIterator>
        inline int64 str_to_big_signed(ConstIterator str, ConstIterator end, error_code & out_error) noexcept {
            // returned on error
            static constexpr int64 NO_RESULT = std::numeric_limits<int64>::max();

            int64 sign;
            if (*str != '-') {
                sign = 1;
            } else {
                sign = -1;
                str += 1;
            }
            uint64 unsigned_value = str_to_big_unsigned<ConstIterator>(str, end, out_error);
            if (out_error) {
                return NO_RESULT;
            }
            static const auto int64_boundary = static_cast<uint64>(std::numeric_limits<int64>::max() + 1);
            if (unsigned_value < int64_boundary) {
                return unsigned_value * sign;
            }
            if (unsigned_value == int64_boundary && sign == -1) {
                return -unsigned_value;
            }
            out_error = error::numeric_overflow;
            return NO_RESULT;
        }


        template <typename UnsignedT, class ConstIterator,
                  class = typename std::enable_if<std::is_unsigned<UnsignedT>::value>::type>
        UnsignedT str_to_unsigned(ConstIterator str, ConstIterator end, error_code & out_error) noexcept {
            // returned on error
            static constexpr auto NO_RESULT = std::numeric_limits<UnsignedT>::max();

            auto u64_value = str_to_big_unsigned<ConstIterator>(str, end, out_error);
            if (not out_error) {
                if (u64_value <= std::numeric_limits<UnsignedT>::max()) {
                    return static_cast<UnsignedT>(u64_value);
                } else {
                    out_error = error::numeric_overflow;
                    return NO_RESULT;
                }
            } else {
                return NO_RESULT;
            }
        }

        template <typename SignedT, class ConstIterator,
        class = typename std::enable_if<std::is_signed<SignedT>::value>::type>
        SignedT str_to_signed(ConstIterator str, ConstIterator end, error_code & out_error) noexcept {
            // returned on error
            static constexpr auto NO_RESULT = std::numeric_limits<SignedT>::max();

            auto s64_value = str_to_big_signed<ConstIterator>(str, end, out_error);
            if (not out_error) {
                if (std::numeric_limits<SignedT>::min() <= s64_value && s64_value <= std::numeric_limits<SignedT>::max()) {
                    return static_cast<SignedT>(s64_value);
                } else {
                    out_error = error::numeric_overflow;
                    return NO_RESULT;
                }
            } else {
                return NO_RESULT;
            }
        }


        template <typename IntegerT, class ConstIterator, bool IsSigned>
        struct choose_str_to_int;

        template <typename IntegerT, class ConstIterator>
        struct choose_str_to_int<IntegerT, ConstIterator, false> {
            static IntegerT convert(ConstIterator str, ConstIterator end, error_code & out_error) noexcept { return str_to_unsigned<IntegerT>(str, end, out_error); }
        };

        template <typename IntegerT, class ConstIterator>
        struct choose_str_to_int<IntegerT, ConstIterator, true> {
            static IntegerT convert(ConstIterator str, ConstIterator end, error_code & out_error) noexcept { return str_to_signed<IntegerT>(str, end, out_error); }
        };

    } // namespace internal


    /// convert decimal number from ASCII string to the native integer type
    template <typename Integer, class ConstIterator>
    inline Integer str_to_int(ConstIterator str, ConstIterator end, error_code & out_error) noexcept {
        return internal::choose_str_to_int<Integer, ConstIterator, std::is_signed<Integer>::value>::convert(str, end, out_error);
    }


    /// @copydoc str_to_int()
    /// may throw `system_error`
    template <typename Integer, class ConstIterator>
    inline Integer str_to_int(ConstIterator str, ConstIterator end) {
        static_assert(std::is_integral<Integer>::value, "Non-integer type");
        error_code err_code;
        auto result = internal::choose_str_to_int<Integer, ConstIterator, std::is_signed<Integer>::value>::convert(str, end, err_code);
        if (not err_code) {
            return result;
        } else {
            throw system_error(err_code);
        }
    }
    

    /// On-stack buffer to hold the ASCII string representing integer number
    typedef char AsciiIntegerBuffer[internal::numeric<int64>::max_str_length];

    /// @}



} // namespace cachelot

/// @}

#endif // CACHELOT_STRING_CONV_H_INCLUDED

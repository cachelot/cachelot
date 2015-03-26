#ifndef CACHELOT_STRING_CONV_H_INCLUDED
#define CACHELOT_STRING_CONV_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup common
/// @{

namespace cachelot {

    /// @defgroup conv Type to string and vice versa conversion
    /// @{

    namespace internal {
        template <typename IntType> struct numeric;
        template <> struct numeric<int8> { typedef uint8 unsigned_; static const size_t max_str_length = 4; };
        template <> struct numeric<int16> { typedef uint16 unsigned_; static const size_t max_str_length = 6; };
        template <> struct numeric<int32> { typedef uint32 unsigned_; static const size_t max_str_length = 11; };
        template <> struct numeric<int64> { typedef uint64 unsigned_; static const size_t max_str_length = 20; };
        template <> struct numeric<uint8> { typedef uint8 unsigned_; static const size_t max_str_length = 3; };
        template <> struct numeric<uint16> { typedef uint16 unsigned_; static const size_t max_str_length = 5; };
        template <> struct numeric<uint32> { typedef uint32 unsigned_; static const size_t max_str_length = 10; };
        template <> struct numeric<uint64> { typedef uint64 unsigned_; static const size_t max_str_length = 20; };

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
    inline size_t int_to_str(IntType value, OutputIterator dest) noexcept {
        static_assert(std::is_integral<IntType>::value, "Non-integer type");
        return internal::int_to_str_impl::convert<IntType, OutputIterator>(value, dest);
    }

    namespace internal {

        // Wrapper on std::strto(u)ll to validate convertion result
        template <typename BigIntType, typename UnderlyingFun>
        inline BigIntType checked_str_to_bigint(const char * str, const size_t length, UnderlyingFun convert_fn) {
            errno = 0;
            char * end_of_convertion;
            const int base = 10;
            auto result = convert_fn(str, &end_of_convertion, base);
            if (errno == 0 && end_of_convertion == str + length) {
                return result;
            } else {
                if (errno == ERANGE) {
                    throw std::range_error("integer value is too big");
                } else {
                    throw std::invalid_argument("invalid integer value");
                }
            }
        }

        template <typename IntType, bool Signed> struct strtoint;

        // `strtoint` specialization to convert ASCII string to signed integer
        template <typename IntType> struct strtoint<IntType, true> {
            static IntType convert(const char * str, const size_t length) {
                static_assert(std::is_signed<IntType>::value, "IntType must be signed integer");
                if (length == 0 || str == nullptr) {
                    throw std::invalid_argument("empty string");
                }
                auto number = checked_str_to_bigint<long long>(str, length, std::strtoll);
                if (number >= std::numeric_limits<IntType>::min() && number <= std::numeric_limits<IntType>::max()) {
                    return static_cast<IntType>(number);
                } else {
                    throw std::range_error("integer value is out of range");
                }
            }
        };

        // `strtoint` specialization to convert ASCII string to unsigned integer
        template <typename UIntType> struct strtoint<UIntType, false> {
            static UIntType convert(const char * str, const size_t length) {
                static_assert(std::is_unsigned<UIntType>::value, "IntType must be unsigned integer");
                if (length == 0 || str == nullptr) {
                    throw std::invalid_argument("empty string");
                }
                auto number = checked_str_to_bigint<unsigned long long>(str, length, std::strtoull);
                // strtoull treats -1 as ULLONG_MAX (see here for details https://groups.google.com/forum/#!topic/comp.std.c/KOVzuLFen6Q)
                // so we must ensure that string doesn't start with '-'
                const char * ch = str;
                while (ch < (str + length) && std::isspace(*ch)) { ch += 1; } // eat white space
                debug_assert(ch < (str + length)); // std::invalid_argument would be thrown otherwise
                if (*ch == '-') {
                    throw std::range_error("integer value is out of range");
                }
                if (number <= std::numeric_limits<UIntType>::max()) {
                    return static_cast<UIntType>(number);
                } else {
                    throw std::range_error("integer value is out of range");
                }
            }
        };

    } // namespace internal

    /// convert C string `str` of given `length` into `Integer` type
    /// @note function may read `str` ahead of `length` (it reads until first non-digit character)
    template <typename Integer>
    inline Integer str_to_int(const char * str, const size_t length) {
        static_assert(std::is_integral<Integer>::value, "Non-integer type");
        return internal::strtoint<Integer, std::is_signed<Integer>::value>::convert(str, length);
    }
    
    /// @}
    


} // namespace cachelot

/// @}

#endif // CACHELOT_STRING_CONV_H_INCLUDED

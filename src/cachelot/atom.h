#ifndef CACHELOT_ATOM_H_INCLUDED
#define CACHELOT_ATOM_H_INCLUDED

/// @ingroup common
/// @{

namespace cachelot {

    namespace internal { 
        
        // encodes ASCII characters to 6bit encoding
        constexpr unsigned char encoding_table[] = {
        /*     ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F  */
        /* 0.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        /* 1.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        /* 2.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        /* 3.. */  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 0,  0,  0,  0,  0,  0,
        /* 4.. */  0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
        /* 5.. */ 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,  0,  0,  0,  0, 37,
        /* 6.. */  0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
        /* 7.. */ 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  0,  0,  0,  0,  0};

        // decodes 6bit characters to ASCII
        constexpr char decoding_table[] = " 0123456789"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                          "abcdefghijklmnopqrstuvwxyz";

        constexpr uint64_t next_interim(uint64_t current, size_t char_code) {
            static_assert(encoding_table[(char_code <= 0x7F) ? char_code : 0] != 0, "Invalid character in the atom");
            return (current << 6) | encoding_table[(char_code <= 0x7F) ? char_code : 0];
        }

        constexpr uint64_t atom_val(const char* cstr, uint64_t interim = 0) {
            return (*cstr == '\0') ? interim : atom_val(cstr + 1, next_interim(interim, static_cast<size_t>(*cstr)));
        }

    } // namespace internal

    /**
     * Atom is an unique number created from character literal.
     *
     * Internaly atom is 6-bit encoded string placed in `uint64` bits
     * Credits to: [C++ Actor Framewor](http://actor-framework.org)
     * @note maximal atom length is limited to 10 characters
     */
    typedef uint64 atom_type;

    /**
     * Returns `what` as a string representation.
     */
    std::string to_string(const atom_type& what);

    /**
     * Creates an atom from given string literal.
     */
    template <size_t Size>
    constexpr atom_type atom(char const (&str)[Size]) {
      // last character is the NULL terminator
      static_assert(Size <= 11, "only 10 characters are allowed");
      return static_cast<atom_type>(internal::atom_val(str, 0xF));
    }

} // namespace cachelot

/// @}

#endif // CACHELOT_ATOM_H_INCLUDED

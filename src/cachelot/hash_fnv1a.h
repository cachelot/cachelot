#ifndef CACHELOT_HASH_FNV1A_H_INCLUDED
#define CACHELOT_HASH_FNV1A_H_INCLUDED

#include <cachelot/bytes.h>

/**
 * @ingroup common
 * @{
 */

namespace cachelot {

    namespace internal {

        template <typename HashType, HashType Seed, HashType Prime>
        class fnv1a_hasher {
        public:
            typedef HashType hash_type;
            hash_type operator()(const bytes data) const noexcept {
                hash_type checksum = Seed;
                for (uint8 one_byte : data) {
                    checksum = (checksum ^ one_byte) * Prime;
                }
                return checksum;
            }
        };

        // 32-bit variant
        typedef internal::fnv1a_hasher<uint32, 0x811c9dc5, 0x01000193> fnv1a_32;
        // 64-bit variant
        typedef internal::fnv1a_hasher<uint64, 0xcbf29ce484222325, 0x100000001b3> fnv1a_64;

        template <size_t NumHashBits> struct fnv1a_choose;
        template <> struct fnv1a_choose<32> { typedef fnv1a_32 hasher; };
        template <> struct fnv1a_choose<64> { typedef fnv1a_64 hasher; };
    }

    /**
     * Fowler–Noll–Vo hash function implementation
     *
     * @tparam HashType - unsigned integral type of a hash (`uint32` or `uint64`)
     * @par Example
     * @code
     * typedef fnv1a<uint32>::hasher hasher_type;
     * auto hash_function = hasher_type();
     * uint32 hash = hash_function(some_data);
     * @endcode
     * @see [FNV-1a on Wikipedia](http://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function#FNV-1a_hash)
     */
    template <typename HashType,
              class = typename std::enable_if<std::is_unsigned<HashType>::value>::type>
    struct fnv1a { typedef typename internal::fnv1a_choose<sizeof(HashType) * 8>::hasher hasher; };


} // namespace cachelot

/// @}

#endif // CACHELOT_HASH_FNV1A_H_INCLUDED

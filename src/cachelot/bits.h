#ifndef CACHELOT_BITS_H_INCLUDED
#define CACHELOT_BITS_H_INCLUDED

/// @ingroup common
/// @{

namespace cachelot {

    namespace internal {
        // count leading zeroes family for 32 / 64 bit integers
        template <typename Int32_T> constexpr unsigned clz32(Int32_T i) noexcept { return __builtin_clz(i); }
        template <typename Int64_T> constexpr unsigned clz64(Int64_T i) noexcept { return __builtin_clzll(i); }

        template <typename IntType,
                  typename std::enable_if<std::is_integral<IntType>::value and sizeof(IntType) < 4>::type * = nullptr>
        constexpr unsigned clz(IntType i) noexcept { return clz32<uint32>(static_cast<uint32>(i)) - (sizeof(uint32) - sizeof(IntType)) * 8; }

        template <typename IntType,
                  typename std::enable_if<std::is_integral<IntType>::value and sizeof(IntType) == 4>::type * = nullptr>
        constexpr unsigned clz(IntType i) noexcept { return clz32<IntType>(i); }

        template <typename IntType,
                  typename std::enable_if<std::is_integral<IntType>::value and sizeof(IntType) == 8>::type * = nullptr>
        constexpr unsigned clz(IntType i) noexcept { return clz64<IntType>(i); }

    }

     /// @defgroup bit Bit manipulations
     /// @{

    /// common bit arithmetics
    namespace bit {

        /// Test whether `bitno` is set (`bitno` counts from zero)
        template <typename IntType>
        constexpr bool isset(const IntType value, const unsigned bitno) noexcept {
            return value & (1 << bitno);
        }

        /// Test whether `bitno` is not set (`bitno` counts from zero)
        template <typename IntType>
        constexpr bool isunset(const IntType value, const unsigned bitno) noexcept {
            return not isset(value, bitno);
        }

        /// Set given `bitno` to 1 (`bitno` counts from zero)
        template <typename IntType>
        constexpr IntType set(const IntType value, const unsigned bitno) noexcept {
            return value | (IntType(1) << bitno);
        }

        /// Set given `bitno` to 0 (`bitno` counts from zero)
        template <typename IntType>
        constexpr IntType unset(const IntType value, const unsigned bitno) noexcept {
            return value & ~(IntType(1) << bitno);
        }

        /// Reverse given `bitno` (`bitno` counts from zero)
        template <typename IntType>
        constexpr IntType flip(const IntType value, const unsigned bitno) noexcept {
            return value ^ (IntType(1) << bitno);
        }

        /// Retrieve no of the most significant bit (`value` must be non-zero)
        template <typename IntType>
        constexpr unsigned most_significant(IntType value) noexcept {
            return (sizeof(IntType) * 8) - internal::clz<IntType>(value) - 1;
        }

    } // namespace bit


    /// Check whether given `n` is a power of 2
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    constexpr bool ispow2(const UIntT n) noexcept {
        return (n > 0) && ((n & (n - 1u)) == 0);
    }


    /// exponentiate a 2 by given `value`
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    constexpr UIntT pow2(const UIntT value) noexcept {
        return (UIntT(1) << value);
    }

    /// return rounded logarithm to the base 2 of given `value`
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    constexpr UIntT log2u(const UIntT value) noexcept {
        return bit::most_significant(value);
    }

    /// round up given `value` to the power of 2
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    inline UIntT roundup_pow2(const UIntT value) noexcept {
        debug_assert(value > 0);
        return ispow2(value) ? value : pow2(log2u(value) + 1u);
    }

    /// determine whether given `alignment` is valid
    template <typename UIntT,
              class = typename std::enable_if<std::is_unsigned<UIntT>::value>::type>
    constexpr UIntT valid_alignment(const UIntT alignment) noexcept {
        return (alignment == (UIntT(1)) || (sizeof(void *) <= alignment)) && ispow2(alignment);
    }

    /// return number of bytes necessary to align `size` according to the given `alignment`
    inline size_t unaligned_bytes(const size_t size, const size_t alignment) noexcept {
        return reinterpret_cast<size_t>((size + (alignment - 1u)) & -alignment) - size;
    }

    /// return number of bytes necessary to align given `addr` according to the given `alignment`
    inline size_t unaligned_bytes(const void * const addr, const size_t alignment) noexcept {
        return unaligned_bytes(reinterpret_cast<const size_t>(addr), alignment);
    }

    /// @}



} // namespace cachelot

/// @}

#endif // CACHELOT_BITS_H_INCLUDED


#ifndef CACHELOT_XRANGE_H_INCLUDED
#define CACHELOT_XRANGE_H_INCLUDED

#include <boost/iterator/counting_iterator.hpp>

/**
 * @ingroup common
 * @{
 */

namespace cachelot {

    template <typename UnderlyingType>
    struct xrange_type {
        typedef UnderlyingType value_type;
        typedef boost::counting_iterator<UnderlyingType> const_iterator;

        constexpr explicit xrange_type(UnderlyingType b, UnderlyingType e) noexcept
            : m_begin(b)
            , m_end(e) {
        }

        constexpr const_iterator begin() const noexcept { return const_iterator(m_begin); }

        constexpr const_iterator end() const noexcept { return const_iterator(m_end); }

    private:
        UnderlyingType m_begin;
        UnderlyingType m_end;
    };

    template <typename UnderlyingType>
    constexpr inline xrange_type<UnderlyingType> xrange(const UnderlyingType end) noexcept {
        return xrange_type<UnderlyingType>(UnderlyingType(), end);
    }

    template <typename UnderlyingType>
    constexpr inline xrange_type<UnderlyingType> xrange(const UnderlyingType begin, const UnderlyingType end) noexcept {
        return xrange_type<UnderlyingType>(begin, end);
    }

//    template <typename UnderlyingType>
//    constexpr inline xrange_type<UnderlyingType> reverse(const xrange_type<UnderlyingType> range) {
//        return
//    }


} // namespace cachelot

/// @}

#endif // CACHELOT_XRANGE_H_INCLUDED

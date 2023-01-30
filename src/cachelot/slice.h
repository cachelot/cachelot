#ifndef CACHELOT_SLICE_H_INCLUDED
#define CACHELOT_SLICE_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//


namespace cachelot {

    /**
     *  @brief references continuous sequence of bytes in memory
     *
     *  slice doesn't own memory, only references external memory.
     *  User is responsible for allocation / deallocation and
     *  for providing valid pointers
     *
     *  @ingroup common
     */
    class slice {
        static_assert(sizeof(char) == 1, "char type must be 1 byte");
    public:
         /// @name constructors
         ///@{

        constexpr slice() noexcept
            : m_begin(nullptr)
            , m_end(nullptr) {
        }

        explicit slice(const char * range_begin, const char * range_end) noexcept
            : m_begin(range_begin)
            , m_end(range_end) {
            debug_assert(m_begin <= m_end);
        }

        constexpr explicit slice(const char * range_begin, const size_t range_length) noexcept
            : m_begin(range_begin)
            , m_end(range_begin + range_length) {
        }
        ///@}

        /// returns `index` byte from range
        char operator[] (size_t index) const noexcept {
            return *nth(index);
        }

        /// returns `index` byte from range
        const char * nth(size_t index) const noexcept {
            debug_assert(index < length());
            return m_begin + index;
        }

        /// returns begin of the range (STL compatible function)
        constexpr const char * begin() const noexcept { return m_begin; }

        /// returns end of the range (STL compatible function)
        constexpr const char * end() const noexcept { return m_end; }

        /// length of the range
        constexpr size_t length() const noexcept { return static_cast<size_t>(m_end - m_begin); }

        /// check if range is empty
        constexpr bool empty() const noexcept { return m_begin == m_end; }

        /// conversion to bool, empty range treat as `false`
        constexpr explicit operator bool() const noexcept {
            return ! empty();
        }

        /// check if this range contents are the same as `other` (memory comparisson)
        bool operator==(const slice & other) const noexcept {
            if (length() == other.length()) {
                if (begin() == other.begin() && end() == other.end()) {
                    return true;
                } else {
                    return std::equal(begin(), end(), other.begin());
                }
            }
            return false;
        }

        /// check if this range contents are the same as `other` (memory comparisson)
        bool operator!=(const slice & other) const noexcept {
            return ! operator==(other);
        }

        /// search for given range `what` in this slice
        slice search(const slice what) const noexcept {
            if (what.length() <= length()) {
                if (what.begin() >= begin() && what.end() <= end()) {
                    return what;
                } else {
                    const char * pos = std::search(begin(), end(), what.begin(), what.end());
                    if (pos != end()) {
                        return slice(pos, what.length());
                    }
                }
            }
            return slice();
        }

        /// returns `true` if this slice contents contains given `subrange`
        bool contains(const slice subrange) const noexcept {
            return search(subrange) ? true : false;
        }

        /// check if pointer `p` is whithin this slice range
        constexpr bool contains(const char * b) const noexcept {
            return (b >= begin() && b < end());
        }

        /// returns `true` if this slice contents starts with `what`
        bool startswith(const slice what) const noexcept {
            return search(what).begin() == begin();
        }

        /// returns `true` if this slice contents ends with `what`
        bool endswith(const slice what) const noexcept {
            if (what.length() <= length()) {
                slice _, xtail;
                tie(_, xtail) = split_at(length() - what.length());
                return xtail == what;
            }
            return false;
        }

        /// return slice containing piece of this slice range started on `index` length of `len`
        slice subslice(size_t index, size_t len) const noexcept {
            debug_assert(nth(index) + len <= m_end);
            return slice(nth(index), len);
        }


        /// shorten this range by `n` from the end
        slice rtrim_n(const size_t n) const noexcept {
            debug_assert(n <= length());
            return subslice(0, length() - n);
        }


        /// split this range by first occurrence of `separator` and return pieces before and after this occurrence
        /// if separator not found split will return this slice followed by empty range
        tuple<slice, slice> split(const slice separator) const noexcept {
            const slice found_sep = search(separator);
            if (found_sep) {
                return make_tuple(slice(begin(), found_sep.begin()), slice(found_sep.end(), end()));
            } else {
                return make_tuple(*this, slice());
            }
        }

        /// @copydoc split()
        tuple<slice, slice> split(const char separator) const noexcept {
            return split(slice(&separator, 1));
        }

        /// split slice at given position (`at` must be within this slice range)
        tuple<slice, slice> split_at(const char * at) const noexcept {
            debug_assert(at);
            debug_assert(at >= begin() && at < end());
            return make_tuple(slice(begin(), at), slice(at, end()));
        }

        /// split slice at the given index (`index` must be less than this range length)
        tuple<slice, slice> split_at(const size_t index) const noexcept {
            const char * split_pos = nth(index);
            return split_at(split_pos);
        }

        /// create string from this slice contents
        string str() const {
            return string(begin(), end());
        }

        /**
         * construct slice from an ASCII string literal
         * @code
         * slice b = slice.from_literal("Some text here");
         * @endcode
         * @note resulting slice will not include leading zero char
         */
        template <size_t N>
        static constexpr slice from_literal(const char (&literal)[N]) {
            return slice(literal, N-1); // leave out '\0' terminator
        }

    private:
        const char * m_begin;
        const char * m_end;
    };


} // namespace cachelot

#endif // CACHELOT_SLICE_H_INCLUDED

#ifndef CACHELOT_BYTES_H_INCLUDED
#define CACHELOT_BYTES_H_INCLUDED

/// @ingroup common
/// @{

namespace cachelot {

    /**
     *  @brief references continuous sequence of bytes in memory
     *
     *  bytes doesn't own memory, only references external memory.
     *  User is responsible for allocation / deallocation and
     *  for providing valid pointers
     */
    class bytes {
        static_assert(sizeof(char) == 1, "char type must be 1 byte");
    public:
         /// @name constructors
         ///@{

        constexpr bytes() noexcept
            : m_begin(nullptr)
            , m_end(nullptr) {
        }

        explicit bytes(const char * range_begin, const char * range_end) noexcept
            : m_begin(range_begin)
            , m_end(range_end) {
            debug_assert(m_begin <= m_end);
        }

        constexpr explicit bytes(const char * range_begin, const size_t range_length) noexcept
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
            return not empty();
        }

        /// check if this range contents are the same as `other` (memory comparisson)
        bool operator==(const bytes & other) const noexcept {
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
        bool operator!=(const bytes & other) const noexcept {
            return not operator==(other);
        }

        /// search for given range `what` in this bytes
        bytes search(const bytes what) const noexcept {
            if (what.length() <= length()) {
                if (what.begin() >= begin() && what.end() <= end()) {
                    return what;
                } else {
                    const char * pos = std::search(begin(), end(), what.begin(), what.end());
                    if (pos != end()) {
                        return bytes(pos, what.length());
                    }
                }
            }
            return bytes();
        }

        /// returns `true` if this bytes contents contains given `subrange`
        bool contains(const bytes subrange) const noexcept {
            return search(subrange) ? true : false;
        }

        /// check if pointer `p` is whithin this bytes range
        constexpr bool contains(const char * b) const noexcept {
            return (b >= begin() && b < end());
        }

        /// returns `true` if this bytes contents starts with `what`
        bool startswith(const bytes what) const noexcept {
            return search(what).begin() == begin();
        }

        /// returns `true` if this bytes contents ends with `what`
        bool endswith(const bytes what) const noexcept {
            if (what.length() <= length()) {
                bytes _, xtail;
                tie(_, xtail) = split_at(length() - what.length());
                return xtail == what;
            }
            return false;
        }

        /// return bytes containing piece of this bytes range started on `index` length of `len`
        bytes slice(size_t index, size_t len) const noexcept {
            debug_assert(nth(index) + len <= m_end);
            return bytes(nth(index), len);
        }


        /// shorten this range by `n` from the end
        bytes rtrim_n(const size_t n) const noexcept {
            debug_assert(n <= length());
            return slice(0, length() - n);
        }


        /// split this range by first occurrence of `separator` and return pieces before and after this occurrence
        /// if separator not found split will return this bytes followed by empty range
        tuple<bytes, bytes> split(const bytes separator) const noexcept {
            const bytes found_sep = search(separator);
            if (found_sep) {
                return make_tuple(bytes(begin(), found_sep.begin()), bytes(found_sep.end(), end()));
            } else {
                return make_tuple(*this, bytes());
            }
        }

        /// @copydoc split()
        tuple<bytes, bytes> split(const char separator) const noexcept {
            return split(bytes(&separator, 1));
        }

        /// split bytes at given position (`at` must be within this bytes range)
        tuple<bytes, bytes> split_at(const char * at) const noexcept {
            debug_assert(at);
            debug_assert(at >= begin() && at < end());
            return make_tuple(bytes(begin(), at), bytes(at, end()));
        }

        /// split bytes at given index (`index` must be less than this range length)
        tuple<bytes, bytes> split_at(const size_t index) const noexcept {
            const char * split_pos = nth(index);
            return split_at(split_pos);
        }

        /// create string from this bytes contents
        string str() const {
            return string(begin(), end());
        }

        /**
         * construct bytes from an ASCII string literal
         * @code
         * bytes b = bytes.from_literal("Some text here");
         * @endcode
         * @note resulting bytes will not include leading zero char
         */
        template <size_t N>
        static bytes from_literal(const char (&literal)[N]) {
            return bytes(literal, N-1); // leave out '\0' terminator
        }

    private:
        const char * m_begin;
        const char * m_end;
    };


} // namespace cachelot

/// @}

#endif // CACHELOT_BYTES_H_INCLUDED

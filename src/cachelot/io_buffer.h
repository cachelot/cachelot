#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#define CACHELOT_IO_BUFFER_H_INCLUDED

#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif

/**
 * @defgroup io IO
 * @{
 */

namespace cachelot {

    // constants
    constexpr size_t default_min_buffer_size = 2048; // 2Kb
    constexpr size_t default_max_buffer_size = 1024 * 1024 * 32; // 32Mb


    /**
     * Dynamically growing (up to `max_size`) buffer for async IO
     * maintains read and write positions
     *
     * To simplify io_buffer usage in asynchronous operations
     * read and write operation consist of two phases:
     *
     * read:
     *  - get all unread data with unread() call
     *  - mark N bytes as read by calling read()
     *
     * write:
     *  - get write pointer in buffer by calling begin_write() and
     *  - mark N bytes as filled by calling confirm()
     */
    class io_buffer {
        typedef std::unique_ptr<char[]> underlying_array_type;
    public:
        // ctor / dtor
        explicit io_buffer(const size_t initial_size, const size_t max_size)
            : m_max_size(max_size) {
            ensure_capacity(initial_size);
        }
        ~io_buffer() = default;
        // disallowed copy and aasignment
        io_buffer(const io_buffer & ) = delete;
        io_buffer & operator= (const io_buffer & ) = delete;

        /// total buffer capacity
        size_t capacity() const noexcept { return m_capacity; }

        /// number of written bytes
        size_t size() const noexcept { return m_write_pos; }

        /// nuber of unread bytes
        size_t unread() const noexcept {
            debug_assert(m_write_pos >= m_read_pos);
            return m_write_pos - m_read_pos;
        }

        /// position in buffer to read from
        const char * begin_read() const noexcept {
            debug_assert(m_read_pos <= m_write_pos);
            return m_data.get() + m_read_pos;
        }

        /// mark `n` more bytes as read
        bytes read(const size_t n) noexcept {
            debug_assert((m_read_pos + n) <= m_write_pos);
            bytes result(m_data.get() + m_read_pos, n);
            m_read_pos += n;
            if (m_read_pos == m_write_pos) {
                discard();
            }
            return result;
        }

        /// search for `terminator` and return bytes ending on `terminator` on success or empty bytes otherwise
        bytes try_read_until(const bytes terminator) noexcept {
            debug_assert(terminator); debug_assert(m_read_pos <= m_write_pos);
            bytes search_range(m_data.get() + m_read_pos, unread());
            const bytes found = search_range.search(terminator);
            if (found) {
                bytes result(search_range.begin(), found.end());
                m_read_pos += result.length();
                if (m_read_pos == m_write_pos) {
                    discard();
                }
                return result;
            }
            return bytes();
        }


        /// positinon in buffer to write to
        char * begin_write(const size_t at_least = default_min_buffer_size) {
            ensure_capacity(at_least);
            return m_data.get() + m_write_pos;
        }

        /// mark `num_bytes` as written
        void confirm(const size_t num_bytes) {
            debug_assert(m_write_pos + num_bytes <= m_capacity);
            m_write_pos += num_bytes;
        }

        /// number of unfilled bytes in buffer
        size_t available() const noexcept { return m_capacity - m_write_pos; }

        /// reset read and write pos
        void discard() noexcept { m_read_pos = 0; m_write_pos = 0; }

    private:
        size_t capacity_advice(size_t at_least) const noexcept {
            const size_t grow_factor = std::max(at_least, std::max(capacity() * 2 - available(), default_min_buffer_size));
            return std::min(capacity() + grow_factor, m_max_size);
        }

        void grow_to(const size_t new_capacity) {
            if (new_capacity > capacity()) {
                underlying_array_type new_data(new char[new_capacity]);
                if (size() > 0) {
                    std::memcpy(new_data.get(), m_data.get(), size());
                }
                m_data.swap(new_data);
                m_capacity = new_capacity;
            }
        }

        void ensure_capacity(const size_t at_least) {
            if (at_least > available()) {
                const size_t new_capacity = capacity_advice(at_least);
                if (new_capacity - size() < at_least) {
                    throw std::length_error("maximal IO buffer capacity exceeded");
                }
                grow_to(new_capacity);
            }
        }

    private:
        const size_t m_max_size;
        underlying_array_type m_data;
        size_t m_capacity = 0;
        size_t m_read_pos = 0;
        size_t m_write_pos = 0;
    };

} // namespace cachelot

/// @}

#endif // CACHELOT_IO_BUFFER_H_INCLUDED

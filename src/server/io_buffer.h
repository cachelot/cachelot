#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#define CACHELOT_IO_BUFFER_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h>
#endif

/**
 * @defgroup io IO
 * @{
 */

namespace cachelot {

    // constants
    constexpr size_t default_min_buffer_size = 500;
    constexpr size_t default_max_buffer_size = 1024 * 1024 * 30; // ~30Mb


    /**
     * Dynamically growing (up to `max_size`) buffer for async IO
     * maintains read and write positions
     *
     * To simplify io_buffer usage in asynchronous operations
     * read and write operation consist of two phases:
     *
     * read:
     *  - get size in bytes of non_read data with non_read()
     *  - get pointer to the beginning of unread data with begin_read()
     *  - mark N bytes as read by calling confirm_read()
     *
     * write:
     *  - get write pointer in buffer by calling begin_write() and
     *  - mark N bytes as filled by calling confirm_write()
     */
    class io_buffer {
        typedef std::unique_ptr<char[]> underlying_array_type;

        enum class write_state_type : size_t { DUMMY };
        enum class read_state_type : size_t { DUMMY };
    public:
        typedef write_state_type write_state;
        typedef read_state_type read_state;

        /// constructor
        explicit io_buffer(const size_t initial_size, const size_t max_size)
            : m_max_size(max_size) {
            if (initial_size > 0) {
                ensure_capacity(initial_size);
            }
        }

        // dtor
        ~io_buffer() = default;
        // disallowed copy and aasignment
        io_buffer(const io_buffer & ) = delete;
        io_buffer & operator= (const io_buffer & ) = delete;

        /// total buffer capacity
        size_t capacity() const noexcept { return m_capacity; }

        /// number of written bytes
        size_t size() const noexcept { return m_write_pos; }

        /// nuber of non-read bytes
        size_t non_read() const noexcept {
            debug_assert(m_write_pos >= m_read_pos);
            return m_write_pos - m_read_pos;
        }

        /// position in the buffer to read from
        const char * begin_read() const noexcept {
            debug_assert(m_read_pos <= m_write_pos);
            return m_data.get() + m_read_pos;
        }

        /// mark `num_bytes` as read
        bytes confirm_read(const size_t num_bytes) noexcept {
            debug_assert((m_read_pos + num_bytes) <= m_write_pos);
            bytes result(m_data.get() + m_read_pos, num_bytes);
            m_read_pos += num_bytes;
            return result;
        }

        /// get the read position to be able to discard one or more reads in the future
        read_state read_savepoint() const noexcept {
            return static_cast<read_state>(m_read_pos);
        }

        /// make bytes unred again up to `savepoint`
        void discard_read(const read_state savepoint) noexcept {
            debug_assert(static_cast<size_t>(savepoint) <= m_read_pos);
            m_read_pos = static_cast<size_t>(savepoint);
            debug_assert(m_read_pos <= m_write_pos);
        }

        /// read all the non-read data
        bytes read_all() noexcept {
            return confirm_read(non_read());
        }

        /// search for `terminator` and return bytes ending on `terminator` on success or empty bytes otherwise
        bytes try_read_until(const bytes terminator) noexcept {
            debug_assert(terminator); debug_assert(m_read_pos <= m_write_pos);
            bytes search_range(m_data.get() + m_read_pos, non_read());
            const bytes found = search_range.search(terminator);
            if (found) {
                bytes result(search_range.begin(), found.end());
                m_read_pos += result.length();
                return result;
            }
            return bytes();
        }

        /// positinon in buffer to write to
        char * begin_write(const size_t at_least = default_min_buffer_size / 4) {
            // TODO: Better buffer growth heuristic
            ensure_capacity(at_least);
            return m_data.get() + m_write_pos;
        }

        /// mark `num_bytes` as written
        void confirm_write(const size_t num_bytes) noexcept {
            debug_assert(m_write_pos + num_bytes <= m_capacity);
            m_write_pos += num_bytes;
        }

        /// get the write position to be able to discard one or more writes in the future
        write_state write_savepoint() const noexcept {
            return static_cast<write_state>(m_write_pos);
        }

        /// forget written data above the `savepoint`
        void discard_written(const write_state savepoint) noexcept {
            debug_assert(static_cast<size_t>(savepoint) <= m_write_pos);
            m_write_pos = static_cast<size_t>(savepoint);
            debug_assert(m_write_pos >= m_read_pos);
        }

        /// number of unfilled bytes in buffer
        size_t available() const noexcept { return m_capacity - m_write_pos; }

        /// forgert reading and writing pos
        void reset() noexcept {
            m_read_pos = 0u;
            m_write_pos = 0u;
        }

        /// free read data to give space to the new data
        void cleanup() noexcept {
            if (m_read_pos == m_write_pos) {
                m_read_pos = 0u;
                m_write_pos = 0u;
            } else {
                debug_assert(m_read_pos < m_write_pos);
                size_t left_unread = m_write_pos - m_read_pos;
                std::memmove(m_data.get(), m_data.get() + m_read_pos, left_unread);
                m_read_pos = 0u;
                m_write_pos = left_unread;
            }
        }

        /// ensure that buffer is capable to store `at_least` bytes; resize if neccessary
        void ensure_capacity(const size_t at_least) {
            debug_assert(at_least > 0);
            if (at_least > available()) {
                const size_t new_capacity = capacity_advice(at_least);
                if (new_capacity - size() >= at_least) {
                    grow_to(new_capacity);
                } else {
                    throw std::length_error("maximal IO buffer capacity exceeded");
                }
            }
        }

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

#ifndef CACHELOT_IO_SERIALIZATION_H_INCLUDED
#define CACHELOT_IO_SERIALIZATION_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_IO_BUFFER_H_INCLUDED
#  include <cachelot/io_buffer.h>
#endif
#ifndef CACHELOT_STRING_CONV_H_INCLUDED
#  include <cachelot/string_conv.h>
#endif

/**
 * @defgroup io IO
 * @{
 */

namespace cachelot {

    /**
     * Generic serialization function, must be specialized for each serializable
     * type and SerializationTag format
     */
    template <class SerializationTag, typename T>
    void do_serialize(io_buffer & buf, const T value);

    /**
     * Utility that allows chain IO from / into @ref io_buffer
     * SerializationTag is a marker to specify serialization format (text / bin / other),
     * it will be used as a template specialization while calling specific
     * serialization function
     */
    template <class SerializationTag>
    class io_stream {
        typedef io_stream<SerializationTag> this_type;
    public:
        constexpr explicit io_stream(io_buffer & buffer) noexcept : m_buf(buffer) {};
        constexpr io_stream(const this_type &) noexcept = default;
        this_type & operator= (const this_type &) noexcept = default;
        ~io_stream() = default;


        template <typename T>
        this_type operator << (const T value) {
            do_serialize<SerializationTag, T>(m_buf, value);
            return *this;
        }

    private:
        io_buffer & m_buf;
    };

    struct text_serialization_tag {};
    struct binary_serialization_tag {};

    inline void do_serialize_bytes(io_buffer & buf, const bytes value) {
        auto dest = buf.begin_write(value.length());
        std::memmove(dest, value.begin(), value.length());
        buf.complete_write(value.length());
    }

    template<>
    inline void do_serialize<text_serialization_tag, bytes>(io_buffer & buf, const bytes value) {
        do_serialize_bytes(buf, value);
    }

    template<>
    inline void do_serialize<binary_serialization_tag, bytes>(io_buffer & buf, const bytes value) {
        do_serialize_bytes(buf, value);
    }

    template<typename IntType>
    inline void do_serialize_integer_text(io_buffer & buf, const IntType x) {
        auto dest = buf.begin_write(internal::numeric<IntType>::max_str_length);
        size_t written = int_to_str(x, dest);
        buf.complete_write(written);
    }

    #define __do_serialize_integer_text_macro(IntType) \
    template<> \
    inline void do_serialize<text_serialization_tag, IntType>(io_buffer & buf, const IntType value) { \
        do_serialize_integer_text<IntType>(buf, value); \
    }

    __do_serialize_integer_text_macro(int8)
    __do_serialize_integer_text_macro(uint8)
    __do_serialize_integer_text_macro(int16)
    __do_serialize_integer_text_macro(uint16)
    __do_serialize_integer_text_macro(int32)
    __do_serialize_integer_text_macro(uint32)
    __do_serialize_integer_text_macro(int64)
    __do_serialize_integer_text_macro(uint64)

    template<>
    inline void do_serialize<text_serialization_tag, char>(io_buffer & buf, const char value) {
        auto dest = buf.begin_write(1);
        *dest = value;
        buf.complete_write(1);
    }

} // namespace cachelot

/// @}

#endif // CACHELOT_IO_SERIALIZATION_H_INCLUDED

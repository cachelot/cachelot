#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#define CACHELOT_CACHE_ITEM_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/bytes.h> // key, value
#endif
#ifndef CACHELOT_BITS_H_INCLUDED
#  include <cachelot/bits.h>  // unaligned_bytes
#endif
#ifndef CACHELOT_EXPIRATION_CLOCK_H_INCLUDED
#  include <cachelot/expiration_clock.h> // expiration time
#endif
#ifndef CACHELOT_SETTINGS_H_INCLUDED
#  include <cachelot/settings.h>
#endif

/// @ingroup cache
/// @{

namespace cachelot {

    namespace cache {

        /**
         * Item represents cache item with key, value, flags, cas, expiration, etc.
         *
         * For the sake of memory economy, Item has tricky placement in memory
         * @code
         * +---------+-------------------------+-------------------------------------?-------+----------?
         * |  Item   |  key as a sequence      |  value as a sequence                |[align]|[optional]|
         * |  struct |  of a[key_length] bytes |  of a [value_length] bytes          |[bytes]|[ CAS_val]|
         * +---------+-------------------------+-------------------------------------?-------+----------?
         * @endcode
         * Item structure is placed first after it
         * the key is placed, it's `key_length` bytes long then
         * item flags as ASCII string (the most of the time it will be '0')
         * the value bytes sequence
         */
        class Item {
        public:
            // There are primary type specifications
            // Modify with care! Memory Layout and Alignment!
            typedef uint32 hash_type;
            typedef uint16 opaque_flags_type;
            typedef ExpirationClock clock;
            typedef clock::time_point expiration_time_point;
            typedef uint64 cas_value_type;
            static constexpr size_t max_key_length = 250; // ! key size is limited to uint8
        private:
            // Important! declaration order affects item size
            const hash_type m_hash; // hash value
            uint32 m_value_length; // length of value [0..MAX_VALUE_LENGTH]
            expiration_time_point m_expiration_time; // when it expires
            opaque_flags_type m_opaque_flags; // user defined item flags
            const uint8 m_key_length; // length of key [1..MAX_KEY_LENGTH]
            bool m_ignore_cas; // does item have a CAS value

        private:
            /// private destructor
            ~Item(); // should never be called

            // disallow copying / moving
            Item(const Item &) = delete;
            Item & operator= (const Item &) = delete;
            Item(Item &&) = delete;
            Item & operator= (Item &&) = delete;
        public:
            /// constructor
            explicit Item(bytes the_key, hash_type the_hash, uint32 value_length, opaque_flags_type the_flags, expiration_time_point expiration_time, cas_value_type cas_value) noexcept;

            /// Destroy existing Item
            static void Destroy(Item * item) noexcept;

            /// return bytes sequence occupied by key
            bytes key() const noexcept;

            /// return hash value
            hash_type hash() const noexcept { return m_hash; }

            /// return bytes sequence occupied by value
            bytes value() const noexcept;

            /// user defined flags
            opaque_flags_type opaque_flags() const noexcept { return m_opaque_flags; }

            /// unique ID used in CAS operation
            cas_value_type cas_value() const noexcept;

            /// check whether Item is expired
            bool is_expired() const noexcept { return m_expiration_time <= clock::now(); }

            /// update item's expiration time
            void touch(expiration_time_point expiration_time) noexcept { m_expiration_time = expiration_time; }

            /// assign value to the item
            void assign_value(bytes the_value) noexcept;

            /// try to append item's value with given `data`
            bool append(const bytes data);

            /// try to prepend item's value with given `data`
            bool prepend(const bytes data);

            /// Calculate total size in bytes required to store provided fields
            static size_t CalcSizeRequired(const bytes the_key, const uint32 value_length, const cas_value_type cas_value) noexcept;

        private:
            // Item must be properly initialized to call following functions
            static size_t KeyOffset(const Item * i) noexcept;
            static size_t ValueOffset(const Item * i) noexcept;
            static size_t CasValueOffset(const Item * i) noexcept;
            static constexpr size_t AlignedCasValueSize(const size_t total_size) noexcept;
            // return range of memory occupied by this Item
            bytes my_memory() const noexcept;
        };


        inline Item::Item(bytes the_key, hash_type the_hash, uint32 value_length, opaque_flags_type the_flags, expiration_time_point expiration, cas_value_type the_cas) noexcept
                : m_hash(the_hash)
                , m_value_length(value_length)
                , m_expiration_time(expiration)
                , m_opaque_flags(the_flags)
                , m_key_length(the_key.length())
                , m_ignore_cas(the_cas == cas_value_type()) {
            debug_assert(unaligned_bytes(this, alignof(Item) == 0));
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(value_length <= settings.cache.max_value_size);
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + KeyOffset(this), the_key.begin(), the_key.length());
        }


        inline bytes Item::key() const noexcept {
            debug_assert(m_key_length > 0);
            auto key_begin = reinterpret_cast<const char *>(this) + KeyOffset(this);
            bytes k(key_begin, key_begin + m_key_length);
            return k;
        }


        inline bytes Item::value() const noexcept {
            const char * value_begin = reinterpret_cast<const char *>(this) + ValueOffset(this);
            bytes v(value_begin, value_begin + m_value_length);
            return v;
        }


        inline Item::cas_value_type Item::cas_value() const noexcept {
            if (m_ignore_cas) {
                return cas_value_type();
            } else {
                auto cas_begin = reinterpret_cast<const char *>(this) + CasValueOffset(this);
                // ensure that pointer is (somehow) valid
                debug_assert( my_memory().contains(bytes(cas_begin, sizeof(cas_value_type))) );
                // ensure proper alignment
                debug_assert(unaligned_bytes(cas_begin, alignof(cas_value_type)) == 0);
                return *reinterpret_cast<const cas_value_type *>(cas_begin);
            }
        }


        inline void Item::assign_value(bytes the_value) noexcept {
            debug_assert(m_value_length == the_value.length());
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + ValueOffset(this), the_value.begin(), the_value.length());
        }


        inline size_t Item::KeyOffset(const Item *) noexcept {
            return sizeof(Item);
        }


        inline size_t Item::ValueOffset(const Item * i) noexcept {
            debug_assert(i != nullptr && i->m_key_length > 0);
            return KeyOffset(i) + i->m_key_length;
        }


        inline size_t Item::CasValueOffset(const Item * i) noexcept {
            size_t value_end = ValueOffset(i) + i->m_value_length;
            debug_assert(not i->m_ignore_cas);
            return value_end + unaligned_bytes(value_end, alignof(cas_value_type));
        }


        inline size_t Item::CalcSizeRequired(const bytes the_key, const uint32 value_length, const cas_value_type cas_value) noexcept {
            debug_assert(the_key.length() > 0);
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(value_length <= settings.cache.max_value_size);
            size_t item_size = sizeof(Item);
            item_size += the_key.length();
            item_size += value_length;
            const bool ignore_cas = (cas_value == cas_value_type());
            if (ignore_cas) {
                return item_size;
            } else {
                size_t cas_alignment = unaligned_bytes(item_size, alignof(cas_value_type));
                return item_size + cas_alignment + sizeof(cas_value_type);
            }
        }


        inline bytes Item::my_memory() const noexcept {
            auto this_ = reinterpret_cast<const char *>(this);
            const size_t my_size = sizeof(Item) + m_key_length + m_value_length +
                            static_cast<uint>(m_ignore_cas) * sizeof(cas_value_type);
            return bytes(this_, my_size);
        }

    } // namepsace cache 

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_ITEM_H_INCLUDED

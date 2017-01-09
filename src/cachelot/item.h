#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#define CACHELOT_CACHE_ITEM_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_SLICE_H_INCLUDED
#  include <cachelot/slice.h> // key, value
#endif
#ifndef CACHELOT_BITS_H_INCLUDED
#  include <cachelot/bits.h>  // unaligned_bytes
#endif
#ifndef CACHELOT_EXPIRATION_CLOCK_H_INCLUDED
#  include <cachelot/expiration_clock.h> // expiration time
#endif


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
         * the value slice sequence
         * @ingroup cache
         */
        class Item {
        public:
            // There are primary type specifications
            // Modify with care! Memory Layout and Alignment!
            typedef uint32 hash_type;
            typedef uint16 opaque_flags_type;
            typedef ExpirationClock clock;
            typedef clock::time_point expiration_time_point;
            typedef uint64 timestamp_type;
            static constexpr uint8 max_key_length = 250; // ! key size is limited to uint8
            static constexpr uint32 max_value_length = std::numeric_limits<uint32>::max();
            static const seconds infinite_TTL;
        private:
            // Important! declaration order affects item size
            const timestamp_type m_timestamp; // timestamp of this item
            const hash_type m_hash; // hash value
            uint32 m_value_length; // length of value [0..MAX_VALUE_LENGTH]
            expiration_time_point m_expiration_time; // when it expires
            opaque_flags_type m_opaque_flags; // user defined item flags
            const uint8 m_key_length; // length of key [1..MAX_KEY_LENGTH]

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
            explicit Item(slice the_key, hash_type the_hash, uint32 value_length, opaque_flags_type the_flags, seconds ttl, timestamp_type the_timestamp) noexcept;

            /// Destroy existing Item
            static void Destroy(Item * item) noexcept;

            /// return slice sequence occupied by key
            slice key() const noexcept;

            /// return hash value
            hash_type hash() const noexcept { return m_hash; }

            /// return slice sequence occupied by value
            slice value() const noexcept;

            /// assign value to the item
            void assign_value(slice the_value) noexcept;

            /// assign value from the two parts
            void assign_compose(slice left, slice right) noexcept;

            /// user defined flags
            opaque_flags_type opaque_flags() const noexcept { return m_opaque_flags; }

            /// re-assign user defined flags
            void set_opaque_flags(opaque_flags_type f) noexcept { m_opaque_flags = f; }

            /// retrieve timestamp of this item
            timestamp_type timestamp() const noexcept { return m_timestamp; }

            /// retrieve expiration time of this item
            expiration_time_point expiration_time() const noexcept { return m_expiration_time; }

            /// retrive number of seconds until expiration
            seconds ttl() const noexcept;

            /// set new time to live
            void set_ttl(seconds secs) noexcept;

            /// check whether Item is expired
            bool is_expired() const noexcept { return m_expiration_time <= clock::now(); }

            /// Calculate total size in slice required to store provided fields
            static size_t CalcSizeRequired(const slice the_key, const size_t value_length) noexcept;

        private:
            // Item must be properly initialized to call following functions
            static size_t KeyOffset(const Item * i) noexcept;
            static size_t ValueOffset(const Item * i) noexcept;
        };


        inline Item::Item(slice the_key, hash_type the_hash, uint32 value_length, opaque_flags_type the_flags, seconds ttl, timestamp_type the_timestamp) noexcept
                : m_timestamp(the_timestamp)
                , m_hash(the_hash)
                , m_value_length(value_length)
                , m_opaque_flags(the_flags)
                , m_key_length(the_key.length()) {
            set_ttl(ttl);
            debug_assert(unaligned_bytes(this, alignof(Item) == 0));
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(value_length <= max_value_length);
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + KeyOffset(this), the_key.begin(), the_key.length());
        }


        inline slice Item::key() const noexcept {
            debug_assert(m_key_length > 0);
            auto key_begin = reinterpret_cast<const char *>(this) + KeyOffset(this);
            slice k(key_begin, key_begin + m_key_length);
            return k;
        }


        inline slice Item::value() const noexcept {
            auto value_begin = reinterpret_cast<const char *>(this) + ValueOffset(this);
            slice v(value_begin, value_begin + m_value_length);
            return v;
        }


        inline void Item::assign_value(slice the_value) noexcept {
            debug_assert(the_value.length() <= m_value_length);
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + ValueOffset(this), the_value.begin(), the_value.length());
            m_value_length = static_cast<decltype(m_value_length)>(the_value.length());
        }


        inline void Item::assign_compose(slice left, slice right) noexcept {
            debug_assert(left.length() + right.length() <= m_value_length);
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + ValueOffset(this), left.begin(), left.length());
            std::memcpy(this_ + ValueOffset(this) + left.length(), right.begin(), right.length());
            m_value_length = static_cast<decltype(m_value_length)>(left.length() + right.length());
        }



        inline seconds Item::ttl() const noexcept {
            if (m_expiration_time == expiration_time_point::max()) {
                return infinite_TTL;
            } else {
                return std::chrono::duration_cast<seconds>(m_expiration_time - clock::now());
            }
        }


        inline void Item::set_ttl(seconds s) noexcept {
            if (s == infinite_TTL) {
                m_expiration_time = expiration_time_point::max();
            } else {
                m_expiration_time = clock::now() + s;
            }
        }


        inline size_t Item::KeyOffset(const Item *) noexcept {
            return sizeof(Item);
        }


        inline size_t Item::ValueOffset(const Item * i) noexcept {
            debug_assert(i != nullptr && i->m_key_length > 0);
            return KeyOffset(i) + i->m_key_length;
        }


        inline size_t Item::CalcSizeRequired(const slice the_key, const size_t value_length) noexcept {
            debug_assert(the_key.length() > 0);
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(value_length <= std::numeric_limits<uint32>::max());
            size_t item_size = sizeof(Item);
            item_size += the_key.length();
            item_size += value_length;
            return item_size;
        }

    } // namepsace cache

} // namespace cachelot


#endif // CACHELOT_CACHE_ITEM_H_INCLUDED

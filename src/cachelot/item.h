#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#define CACHELOT_CACHE_ITEM_H_INCLUDED

#include <cachelot/bytes.h> // key, value
#include <cachelot/bits.h>  // unaligned_bytes
#include <cachelot/expiration_clock.h> // expiration time

/// @ingroup cache
/// @{

namespace cachelot {

    namespace cache {

        /// Initialize Item memory manager
        void InitItemMemory(size_t size_in_bytes);

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
            static constexpr size_t max_key_length = 250;
            static constexpr size_t max_value_length = 128 * 1024 * 1024;
        private:
            //!Important declaration order affects item size
            expiration_time_point m_expiration_time; // when it expires
            const hash_type m_hash; // hash value
            uint32 m_value_length; // length of value [0..MAX_VALUE_LENGTH]
            opaque_flags_type m_opaque_flags; // user defined item flags
            const uint8 m_key_length; // length of key [1..MAX_KEY_LENGTH]
            bool m_ignore_cas; // does item have a CAS value
//            union {
//                cas_value_type maybe_cas;
//                uint8 key_begin_if_no_cas[];
//            };

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
            explicit Item(bytes the_key, hash_type the_hash, bytes the_value, opaque_flags_type the_flags, expiration_time_point expiration_time, cas_value_type cas_value) noexcept;

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

            /// replace existing Item fields with the new ones
            /// @note: caller is responsible for providing enough memory
            void reassign(bytes new_value, opaque_flags_type new_flags, expiration_time_point new_expiration, cas_value_type new_cas) noexcept;

            /// try to append item's value with given `data`
            bool append(const bytes data);

            /// try to prepend item's value with given `data`
            bool prepend(const bytes data);

            /// Calculate total size in bytes required to store provided fields
            static size_t CalcSizeRequired(const bytes the_key, const bytes the_value, const cas_value_type cas_value) noexcept;

        private:
            // Item must be properly initialized to call following functions
            static size_t KeyOffset(const Item * i) noexcept;
            static size_t ValueOffset(const Item * i) noexcept;
            static size_t CasValueOffset(const Item * i) noexcept;
            static constexpr size_t AlignedCasValueSize(const size_t total_size) noexcept;
            // return range of memory occupied by this Item
            bytes my_memory() const noexcept;
        };


        inline Item::Item(bytes the_key, hash_type the_hash, bytes the_value, opaque_flags_type the_flags, expiration_time_point expiration, cas_value_type cas_value) noexcept
                : m_hash(the_hash)
                , m_key_length(the_key.length()) {
            debug_assert(unaligned_bytes(this, alignof(Item) == 0));
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + KeyOffset(this), the_key.begin(), the_key.length());
            reassign(the_value, the_flags, expiration, cas_value);
        }


        inline bytes Item::key() const noexcept {
            debug_assert(m_key_length > 0);
            auto key_begin = reinterpret_cast<const char *>(this) + KeyOffset(this);
            bytes k(key_begin, key_begin + m_key_length);
            debug_assert(my_memory().contains(k));
            return k;
        }


        inline bytes Item::value() const noexcept {
            const char * value_begin = reinterpret_cast<const char *>(this) + ValueOffset(this);
            // ensure that we're whithin item memory bounds
            bytes v(value_begin, value_begin + m_value_length);
            debug_assert(my_memory().contains(v));
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


        inline void Item::reassign(bytes new_value, opaque_flags_type new_flags, expiration_time_point new_expiration, cas_value_type new_cas_value) noexcept {
            debug_assert(key()); // must be already assigned
            debug_assert(new_value.length() <= max_value_length);
            bool ignore_cas = (new_cas_value == cas_value_type());
            m_expiration_time = new_expiration;
            m_value_length = new_value.length();
            m_opaque_flags = new_flags;
            m_ignore_cas = ignore_cas;
            auto this_ = reinterpret_cast<uint8 *>(this);
            std::memcpy(this_ + ValueOffset(this), new_value.begin(), new_value.length());
            if (not ignore_cas) {
                *(this_ + CasValueOffset(this)) = new_cas_value;
            }
        }


        inline size_t Item::KeyOffset(const Item *) noexcept {
            return sizeof(Item);
        }


        inline size_t Item::ValueOffset(const Item * i) noexcept {
            return KeyOffset(i) + i->m_key_length;
        }


        inline size_t Item::CasValueOffset(const Item * i) noexcept {
            size_t value_end = ValueOffset(i) + i->m_value_length;
            debug_assert(not i->m_ignore_cas);
            return value_end + unaligned_bytes(value_end, alignof(cas_value_type));
        }


        inline size_t Item::CalcSizeRequired(const bytes the_key, const bytes the_value, const cas_value_type cas_value) noexcept {
            debug_assert(the_key.length() > 0);
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(the_value.length() <= max_value_length);
            size_t item_size = sizeof(Item);
            item_size += the_key.length();
            item_size += the_value.length();
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

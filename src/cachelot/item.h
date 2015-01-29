#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#define CACHELOT_CACHE_ITEM_H_INCLUDED

#include <cachelot/bytes.h> // key, value
#include <cachelot/bits.h>  // unaligned_bytes

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
        private:
            //!Important declaration order affects item size
            time_point m_expiration_time; // when it expires
            uint32 m_value_length; // length of value [0..MAX_VALUE_LENGTH]
            opaque_flags_type m_opaque_flags; // user defined item flags
            uint8 m_key_length; // length of key [1..MAX_KEY_LENGTH]
            bool m_ignore_cas; // does item have CAS value or it's default

        private:
            /// private constructor
            explicit Item() noexcept {}

            /// private destructor
            ~Item(); // should never be called

            // disallow copying / moving
            Item(const Item &) = delete;
            Item & operator= (const Item &) = delete;
            Item(Item &&) = delete;
            Item & operator= (Item &&) = delete;
        public:
            /// Create new Item
            static Item * Create(bytes the_key, bytes the_value, opaque_flags_type the_flags, time_point expiration_time, cas_value_type cas_value) noexcept;

            /// Destroy existing Item
            static void Destroy(Item * item) noexcept;

            /// return bytes sequence occupied by key
            bytes key() const noexcept;

            /// return bytes sequence occupied by value
            bytes value() const noexcept;

            /// user defined flags
            opaque_flags_type opaque_flags() const noexcept { return m_opaque_flags; }

            /// unique ID used in CAS operation
            cas_value_type cas_value() const noexcept;

            /// check whether Item is expired
            bool is_expired() const noexcept { return m_expiration_time <= clock::now(); }

            /// update item's expiration time
            void touch(time_point expiration_time) noexcept { m_expiration_time = expiration_time; }

            /// try to replace existing value with new and manage memory accordingly
            ///
            /// @return `false` if Item has not enough memory to place new value
            bool replace_value(bytes new_value, opaque_flags_type new_flags, time_point new_expiration, cas_value_type new_cas) noexcept;

            /// try to append item's value with given `data`
            bool append(const bytes data);

            /// try to prepend item's value with given `data`
            bool prepend(const bytes data);

        private:
            /// Allocate contiguous memory for the new Item of requested `size`
            ///
            /// @return `nullptr` if memory allocation fails and pointer to the newly allocated memory otherwise
            static Item * Allocate(const size_t size) noexcept;

            /// Change amount of memory used by existing Item
            ///
            /// @return `true` on success and `false` if amount of memory can't be changed
            /// @note `item` and it's existing memory stays untouched in any case
            static bool ResizeMemory(Item * item, const size_t new_size) noexcept;

            /// Free memory used by `item`
            static void Free(Item * item) noexcept;

            static size_t KeyOffset(const Item * i) noexcept;
            static size_t ValueOffset(const Item * i) noexcept;
            static size_t CasValueOffset(const Item * i) noexcept;
            static constexpr size_t AlignedCasValueSize(const size_t total_size) noexcept;
            static size_t CalcSizeRequired(const bytes the_key, const bytes the_value, const bool ignore_cas) noexcept;
            // return range of memory occupied by this Item
            bytes my_memory() const noexcept;
        };

        inline Item * Item::Create(bytes the_key, bytes the_value, opaque_flags_type the_flags, time_point expiration_time, cas_value_type cas_value) noexcept {
            // no need to store default CAS value
            const bool ignore_cas = (cas_value == cas_value_type());
            const size_t item_size = CalcSizeRequired(the_key, the_value, ignore_cas);
            Item * new_item = Allocate(item_size);
            if (new_item == nullptr) {
                return nullptr;
            }
            debug_assert(unaligned_bytes(new_item, alignof(Item) == 0));
            new_item->m_expiration_time = expiration_time;
            new_item->m_value_length = the_value.length();
            new_item->m_opaque_flags = the_flags;
            new_item->m_key_length = the_key.length();
            new_item->m_ignore_cas = ignore_cas;
            auto new_item_memory = reinterpret_cast<uint8 *>(new_item);
            std::memcpy(new_item_memory + KeyOffset(new_item), the_key.begin(), the_key.length());
            std::memcpy(new_item_memory + ValueOffset(new_item), the_value.begin(), the_value.length());
            if (not ignore_cas) {
                *(new_item_memory + CasValueOffset(new_item)) = cas_value;
            }
            return new_item;
        }

        inline bytes Item::key() const noexcept {
            debug_assert(m_key_length > 0);
            auto key_begin = reinterpret_cast<const char *>(this) + KeyOffset(this);
            bytes key(key_begin, key_begin + m_key_length);
            debug_assert(my_memory().contains(key));
            return key;
        }

        inline bytes Item::value() const noexcept {
            const char * value_begin = reinterpret_cast<const char *>(this) + ValueOffset(this);
            // ensure that we're whithin item memory bounds
            bytes value(value_begin, value_begin + m_value_length);
            debug_assert(my_memory().contains(value));
            return value;
        }

        inline cas_value_type Item::cas_value() const noexcept {
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

        inline bool Item::replace_value(bytes new_value, opaque_flags_type new_flags, time_point new_expiration, cas_value_type new_cas_value) noexcept {
            debug_assert(key()); // must be already assigned
            debug_assert(new_value.length() <= max_value_length);
            bool ignore_cas = (new_cas_value == cas_value_type());
            const size_t new_size = CalcSizeRequired(key(), new_value, ignore_cas);
            if (ResizeMemory(this, new_size)) {
                m_expiration_time = new_expiration;
                m_value_length = new_value.length();
                m_opaque_flags = new_flags;
                m_ignore_cas = ignore_cas;
                auto this_ = reinterpret_cast<uint8 *>(this);
                std::memcpy(this_ + ValueOffset(this), new_value.begin(), new_value.length());
                if (not ignore_cas) {
                    *(this_ + CasValueOffset(this)) = new_cas_value;
                }
                return true;
            } else {
                return false;
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

        inline size_t Item::CalcSizeRequired(const bytes the_key, const bytes the_value, const bool ignore_cas) noexcept {
            debug_assert(the_key.length() <= max_key_length);
            debug_assert(the_value.length() <= max_value_length);
            size_t item_size = sizeof(Item);
            item_size += the_key.length();
            item_size += the_value.length();
            if (ignore_cas) {
                return item_size;
            } else {
                size_t cas_alignment = unaligned_bytes(item_size, alignof(cas_value_type));
                return item_size + cas_alignment + sizeof(cas_value_type);
            }
        }

        inline bytes Item::my_memory() const noexcept {
            auto this_ = reinterpret_cast<const char *>(this);
            const size_t my_size = CalcSizeRequired(key(), value(), m_ignore_cas);
            return bytes(this_, my_size);
        }

    } // namepsace cache 

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_ITEM_H_INCLUDED

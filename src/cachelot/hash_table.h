#ifndef CACHELOT_HASH_TABLE_H_INCLUDED
#define CACHELOT_HASH_TABLE_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <cachelot/bits.h> // is_pow2

namespace cachelot {

    namespace internal {

        template <typename Key, typename T>
        class hash_table_entry {
            typedef hash_table_entry<Key, T> this_type;
        public:
            hash_table_entry() = default;
            explicit hash_table_entry(const Key & the_key, const T & the_value) : m_key(the_key), m_value(the_value) {}
            hash_table_entry(this_type &&) = default;
            hash_table_entry & operator=(this_type &&) = default;
            // disallow copying
            hash_table_entry(const this_type &) = delete;
            hash_table_entry & operator=(const this_type &) = delete;
            // key getter
            const Key & key() const noexcept { return m_key; }
            // value getter
            const T & value() const noexcept { return m_value; }
            // swap with other entry
            void swap(this_type & other) {
                using std::swap;
                swap(m_key, other.m_key);
                swap(m_value, other.m_value);
            }
        private:
            Key m_key;
            T m_value;
        };

        template <typename Key, typename T>
        void swap(hash_table_entry<Key, T> & left, hash_table_entry<Key, T> & right) {
            left.swap(right);
        }

        struct DefaultOptions {
            typedef size_t size_type;
            typedef size_t hash_type;
            static constexpr size_type max_load_factor_percent = 93;
        };

    } // namespace internal


    /**
     * CPU cache friendly hash table implementation based on [Robin Hood algorithm](http://en.wikipedia.org/wiki/Hash_table#Robin_Hood_hashing)
     *
     * @note this is low level implementation class it doesn't support resizing
     * @see dict class
     * @ingroup common
     */
    template <typename Key, typename T, typename KeyEqual = std::equal_to<Key>,
              class Entry = internal::hash_table_entry<Key, T>, class Options = internal::DefaultOptions>
    class hash_table {
    public:
        typedef Key key_type;
        typedef T mapped_type;
        typedef KeyEqual key_equal;
        typedef Entry entry_type;
        typedef typename Options::size_type size_type;
        typedef typename Options::hash_type hash_type;
        static constexpr size_type max_load_factor_percent = Options::max_load_factor_percent;
        static_assert(std::is_unsigned<size_type>::value, "size_type must be unsigned");
        static_assert((max_load_factor_percent > 0) && (max_load_factor_percent < 100), "max_load_factor_percent must be in range (0..100)");
    private:
        // implementation details
        typedef std::unique_ptr<hash_type[]> hash_array_type; // hashes are stored separately to improve D-cache locality
        typedef std::unique_ptr<entry_type[]> entry_array_type;
    public:
        /// constructor
        hash_table(const size_type the_capacity) noexcept
            : m_size(0)
            , m_capacity(the_capacity)
            , mask(the_capacity - 1)
            , m_hashes(new (nothrow) hash_type[the_capacity])
            , m_entries(new (nothrow) entry_type[the_capacity]){
            debug_assert(the_capacity > 0);
            debug_assert(ispow2(the_capacity));
            if (ok()) {
                clear();
            }
        }

        // disallow copying
        hash_table(const hash_table &) = delete;
        hash_table & operator= (const hash_table &) = delete;

        /// retrieve stored value by its key
        tuple<bool, mapped_type> get(const key_type key, const hash_type hash) const noexcept {
            debug_assert(hash != 0);
            bool found; size_type pos;
            tie(found, pos) = entry_for(key, hash);
            mapped_type result = found ? entry_at(pos).value() : mapped_type();
            return tuple<bool, mapped_type>(found, result);
        }

        /// insert key value pair into table or update existing value
        ///
        /// @return true if new value was inserted, false if existing value was updated
        bool put(key_type key, hash_type hash, mapped_type value) noexcept {
            debug_assert(hash != 0);
            bool found; size_type pos;
            tie(found, pos) = entry_for(key, hash);
            if (found) {
                debug_assert(eq(entry_at(pos).key(), key));
                entry_type & curr_entry = entry_at(pos);
                entry_type new_entry(key, value);
                std::swap(curr_entry, new_entry);
                return false;
            } else {
                insert(pos, key, hash, value);
                return true;
            }
        }

        /// remove existing entry
        ///
        /// @return true if value was deleted and false if value was not found
        bool del(key_type key, hash_type hash) noexcept {
            bool found; size_type pos;
            tie(found, pos) = entry_for(key, hash);
            if (found) {
                debug_assert(eq(entry_at(pos).key(), key));
                remove(pos);
                return true;
            } else {
                return false;
            }
        }


        /// remove all entries that satisfy given condifion
        ///
        /// @tparam ConditionFun ```bool do_remove(mapped_type)```
        template <typename ConditionFun>
        void remove_if(ConditionFun predicate) noexcept {
            size_type pos = 0;
            while (pos < capacity()) {
            	if (! empty_at(pos)) {
                    if (predicate(entry_at(pos).value())) {
                        remove(pos);
                        continue;
                    }
                }
                pos += 1;
            }
        }

        /// check if given `key` is in the hash table
        bool contains(key_type key, hash_type hash) const noexcept {
            debug_assert(hash != 0);
            bool found; size_t __;
            tie(found, __) = entry_for(key, hash);
            return found;
        }

        /// retrieve position of in table for the given `key`
        ///
        /// @return either `(true, existing_entry_pos)` if key exists, or `(false, position_to_insert_new_entry)` if key wasn't found
        tuple<bool, size_type> entry_for(const key_type key, const hash_type hash) const noexcept {
            debug_assert(hash != 0);
            size_type pos = desired_position(hash);
            size_type distance = 0;
            // NOTE! we may give up before we find suitable slot, then `insert` will continue search from this point
            // lookup for non-existing item would take O(N) otherwise
            while (! empty_at(pos) && distance <= get_distance(pos, hash_at(pos))) {
                if (hash_at(pos) == hash && eq(entry_at(pos).key(), key)) {
                    return tuple<bool, size_type>(true, pos);
                } else {
                    pos = inc_pos(pos);
                    distance += 1;
                }
            }
            return tuple<bool, size_type>(false, pos);
        }

        /// insert entry starting from given pos that was returned by @ref hash_table::entry_for
        size_type insert(size_type pos, const key_type key, hash_type hash, mapped_type value) noexcept {
            debug_assert(! threshold_reached()); debug_assert(hash != 0);
            entry_type entry(key, value);
            // how far we from the desired position
            size_type lookup_distance = get_distance(pos, hash);
            while (! empty_at(pos)) {
                debug_assert( ! eq(entry_at(pos).key(), entry.key()) ); // entry must be unique
                const size_type existing_entry_distance = get_distance(pos, hash_at(pos));
                if (existing_entry_distance < lookup_distance) {
                    std::swap(hash, m_hashes[pos]);
                    swap(entry, entry_at(pos));
                    lookup_distance = existing_entry_distance;
                }
                pos = inc_pos(pos);
                lookup_distance += 1;
            }
            debug_assert(pos < capacity() && empty_at(pos));
            m_hashes[pos] = hash;
            std::swap(m_entries[pos], entry);
            m_size += 1;
            return pos;
        }

        /// remove element pos given pos
        void remove(const size_type pos) noexcept {
            debug_assert(! empty_at(pos));
            debug_assert(m_size > 0);
            m_hashes[pos] = 0;
            m_size -= 1;
            // optimize following items
            size_type current_position = pos;
            size_type following_position = inc_pos(pos);
            while (! empty_at(following_position) && get_distance(following_position, hash_at(following_position)) > 0) {
                std::swap(m_hashes[current_position], m_hashes[following_position]);
                swap(m_entries[current_position], m_entries[following_position]);
                current_position = inc_pos(current_position);
                following_position = inc_pos(following_position);
            }
        }

        /// clear the hash table
        void clear() noexcept {
            for (size_type pos = 0; pos < capacity(); ++pos) {
                m_hashes[pos] = 0;
            }
            m_size = 0;
        }


        /// return hash value of an entry at `pos`
        hash_type hash_at(const size_type pos) const noexcept {
            debug_assert(pos < capacity());
            return m_hashes[pos];
        }

        /// return entry by its `pos`
        entry_type & entry_at(const size_type pos) noexcept {
            debug_assert(pos < capacity());
            return m_entries[pos];
        }

        /// @copydoc entry_at()
        const entry_type & entry_at(const size_type pos) const noexcept {
            debug_assert(pos < capacity());
            return m_entries[pos];
        }

        /// check whether slot at `pos` is empty
        bool empty_at(const size_type pos) const noexcept {
            return hash_at(pos) == 0;
        }

        /// capacity of a table
        constexpr size_type capacity() const noexcept { return m_capacity; }

        /// number of items in table
        constexpr size_type size() const noexcept { return m_size; }

        /// check whether number of stored elements equals max_size()
        constexpr bool threshold_reached() const noexcept { return m_size >= max_size(); }

        /// check whether all internal elements were successfully initialized
        constexpr bool ok() const noexcept { return m_hashes && m_entries; }

        /// check whether table has no elements
        constexpr bool empty() const noexcept { return m_size == 0; }

        /// returns max number of elements that can be stored in table
        constexpr size_type max_size() const noexcept { return static_cast<size_type>(capacity() * max_load_factor_percent / 100); }

    private:
        /// return position in table which element with given `hash` would occupy
        constexpr size_type desired_position(const hash_type hash) const noexcept { return hash & mask; }

        /// increment position in a table, start from 0 when reaching capacity
        constexpr size_type inc_pos(const size_type pos) const noexcept {
            // if (pos < capacity) pos + 1 else 0
            return (pos + 1) & mask;
        }

        /// return distance from the desired position of the `hash`
        size_type get_distance(const size_type pos, const hash_type hash) const noexcept {
            size_type distance = (pos + capacity() - desired_position(hash)) & mask;
            debug_assert(distance <= m_capacity);
            return distance;
        }

    private:
        size_type m_size;
        const size_type m_capacity;
        const size_type mask;
        hash_array_type m_hashes;
        entry_array_type m_entries;
        const key_equal eq = KeyEqual();
    };

} // namespace cachelot


#endif // CACHELOT_HASH_TABLE_H_INCLUDED

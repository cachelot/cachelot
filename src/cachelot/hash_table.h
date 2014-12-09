#ifndef CACHELOT_HASH_TABLE_H_INCLUDED
#define CACHELOT_HASH_TABLE_H_INCLUDED

#include <cachelot/bits.h> // is_pow2

/// @ingroup common
/// @{

namespace cachelot {

    namespace internal {

        template <typename Key, typename T>
        struct hash_table_traits {
            // How many percents of the table should be occupied before it will be considered as full
            static const size_type max_load_factor_percent = 93;
            typedef size_t size_type;
            typedef size_t hash_type;
            typedef Key key_type;
            typedef T mapped_type;
            // type of the hash_table single element with key and value
            struct entry_type {
                key_type key;
                mapped_type
            };
            // key getter
            static const key_type key_of(const entry_type e) { return e.key; }
            // swap two entries
            static void swap(entry_type && left, entry_type && right) { std::swap(move(left), move(right)); }
        };

    } // namespace internal


    /**
     * CPU cache friendly hash table implementation based on [Robin Hood algorithm](http://en.wikipedia.org/wiki/Hash_table#Robin_Hood_hashing)
     *
     * @note this is low level implementation class it doesn't support resizing
     * @see dict class
     */
    template <typename Key, typename T, class KeyEqual = std::equal_to<Key>, class Traits = internal::hash_table_traits>
    class hash_table {
    public:
        typedef Key key_type;
        typedef T mapped_type;
        typedef size_t size_type;
        typedef Hash hash_type;
        typedef KeyEq key_equal;
    private:
        // implementation details
        typedef std::unique_ptr<hash_type[]> hash_array_type; // hashes are stored separately to improve D-cache locality
        typedef std::unique_ptr<ItemPtr[]> entry_array_type;

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
                for (size_type at = 0; at < capacity(); ++at) {
                    m_hashes[at] = 0;
                }
            }
        }

        // disallow copying
        hash_table(const hash_table &) = delete;
        hash_table & operator= (const hash_table &) = delete;

        /// retrieve stored value by its key
        tuple<bool, mapped_type> get(const key_type key, const hash_type hash) const noexcept {
            debug_assert(hash != 0);
            bool found; size_type at;
            tie(found, at) = entry_for(key, hash);
            mapped_type result = found ? get_entry(at).value : mapped_type();
            return tuple<bool, mapped_type>(found, result);
        }

        /// insert key value pair into table or update existing value
        ///
        /// @return true if new value was inserted, false if existing value was updated
        bool put(key_type key, hash_type hash, mapped_type value) noexcept {
            debug_assert(hash != 0);
            bool found; size_type at;
            tie(found, at) = entry_for(key, hash);
            if (found) {
                debug_assert(eq(get_entry(at).key, key));
                auto & e = get_entry(at);
                e.key = key;
                e.value = value;
                return false;
            } else {
                insert(at, key, hash, value);
                return true;
            }
        }
        
        /// removes existing entry
        ///
        /// @return true if value was deleted and false if value was not found
        bool del(key_type key, hash_type hash) noexcept {
            debug_assert(hash != 0);
            bool found; size_type at;
            tie(found, at) = entry_for(key, hash);
            if (found) {
                debug_assert(eq(get_entry(at).key, key));
                remove(at);
                return true;
            } else {
                return false;
            }
        }
        
        /// check if given `key` is in hash table
        bool contains(key_type key, hash_type hash) const noexcept {
            bool found; size_t __;
            tie(found, __) = entry_for(key, hash);
            return found;
        }
        
        /// retrieve position of in table for the given `key`
        ///
        /// @return either `(true, existing_entry_pos)` if key exists, or `(false, position_to_insert_new_entry)` if key wasn't found
        tuple<bool, size_type> entry_for(const key_type key, const hash_type hash) const noexcept {
            debug_assert(hash != 0);
            size_type at = desired_position(hash);
            size_type distance = 0;
            // NOTE! we may give up before we find suitable slot, then `insert` will continue search from this point
            // lookup for non-existing item would take O(N) otherwise
            while (not empty_slot(at) && distance <= get_distance(at, get_hash(at))) {
                if (get_hash(at) == hash && eq(get_entry(at).key, key)) {
                    return tuple<bool, size_type>(true, at);
                } else {
                    at = next(at);
                    distance += 1;
                }
            }
            return tuple<bool, size_type>(false, at);
        }
        
        /// insert entry starting from given pos that was returned by @ref hash_table::entry_for
        size_type insert(size_type at, const key_type key, hash_type hash, mapped_type value) noexcept {
            debug_assert(not threshold_reached()); debug_assert(hash != 0);
            entry_type entry(key, value);
            // how far we from the desired position
            size_type lookup_distance = get_distance(at, hash);
            while (not empty_slot(at)) {
                debug_assert(not eq(get_entry(at).key, entry.key)); // entry must be unique
                const size_type existing_entry_distance = get_distance(at, get_hash(at));
                if (existing_entry_distance < lookup_distance) {
                    std::swap(hash, m_hashes[at]);
                    std::swap(entry, get_entry(at));
                    lookup_distance = existing_entry_distance;
                }
                at = next(at);
                lookup_distance += 1;
            }
            debug_assert(at < capacity() && empty_slot(at));
            m_hashes[at] = hash;
            m_entries[at] = entry;
            m_size += 1;
            return at;
        }

        /// remove element at given pos
        void remove(const size_type at) noexcept {
            debug_assert(not empty_slot(at));
            m_hashes[at] = 0;
            m_entries[at] = entry_type();
            debug_assert(m_size > 0);
            m_size -= 1;
            // optimize following items
            size_type current_position = at;
            size_type following_position = next(at);
            while (not empty_slot(following_position) && get_distance(following_position, get_hash(following_position)) > 0) {
                std::swap(m_hashes[current_position], m_hashes[following_position]);
                std::swap(m_entries[current_position], m_entries[following_position]);
                current_position = next(current_position);
                following_position = next(following_position);
            }
        }

        hash_type get_hash(const size_type at) const noexcept {
            debug_assert(at < capacity());
            return m_hashes[at];
        }
        
        /// return entry by its pos
        entry_type & get_entry(const size_type at) noexcept {
            debug_assert(at < capacity());
            return m_entries[at];
        }
        
        /// @copydoc get_entry()
        const entry_type & get_entry(const size_type at) const noexcept {
            debug_assert(at < capacity());
            return m_entries[at];
        }
        
        /// check whether entry at given pos is empty
        bool empty_slot(const size_type at) const noexcept {
            return get_hash(at) == 0;
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
        constexpr size_type next(const size_type at) const noexcept {
            // if (at < capacity) at + 1 else 0
            return (at + 1) & mask;
        }

        /// return distance from the desired position of the `hash`
        size_type get_distance(const size_type at, const hash_type hash) const noexcept {
            size_type distance = (at + capacity() - desired_position(hash)) & mask;
            debug_assert(distance <= m_capacity);
            return distance;
        }

    private:
        size_type m_size;
        const size_type m_capacity;
        const size_type mask;
        hash_array_type m_hashes;
        entry_array_type m_entries;
        const key_equal eq = KeyEq();
    };

} // namespace cachelot

/// @}

#endif // CACHELOT_HASH_TABLE_H_INCLUDED

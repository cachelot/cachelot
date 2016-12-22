#ifndef CACHELOT_DICT_H_INCLUDED
#define CACHELOT_DICT_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <cachelot/hash_table.h> // hash_table

namespace cachelot {

    /**
     * dict is an unordered key-value associative container
     *
     * Typical hash table has a problem when adding new element causes resize of table
     * and thus reshash of all elements
     * To impove worst case, dict maintains 2 hash tables - primary and secondary.
     * Normaly all elements are stored in the primary table and there is no secondary table,
     * but when load factor of primary table exceds threshold, primary becomes the secondary,
     * new table allocated as a new primary and every update operation on dict moves some
     * items from the secondary table back to the primary, util no items left in the secondary
     *
     * @note dict does not manage stored items lifetime. It expects items to be POD data with trivial destructor and copy.
     *
     * @tparam Key - key type
     * @tparam T - value type
     * @tparam KeyEqual - key equality predicate
     * @tparam Entry - class representing entry of the hash_table,
     *                 must have following interface:
     * @code
     *  template <typename Key, typename T> struct hash_table_entry {
     *      // constructor
     *      explicit hash_table_entry(const Key & the_key, const T & the_value);
     *      // key getter
     *      const Key & key() const noexcept;
     *      // value getter
     *      const T & value() const noexcept { return m_value; }
     *  };
     * @endcode
     * @tparam Options - options of the hash_table:
     * @code
     *  struct Options {
     *      // size type (must be unsigned integer)
     *      typedef size_t size_type;
     *      // hash value type (must be unsigned integer)
     *      typedef size_t hash_type;
     *      // percentage of hash table fill untill threshold, must be in (0, 100) range
     *      static constexpr size_type max_load_factor_percent = 93;
     *  };
     * @endcode
     *
     * @ingroup common
     */
    template <typename Key, typename T, typename KeyEqual = std::equal_to<Key>,
              class Entry = internal::hash_table_entry<Key, T>, class Options = internal::DefaultOptions>
    class dict {
        typedef hash_table<Key, T, KeyEqual, Entry, Options> hash_table_type;
    public:
        typedef typename hash_table_type::size_type size_type;
        typedef typename hash_table_type::hash_type hash_type;
        typedef typename hash_table_type::key_equal key_equal;
        typedef Key key_type;
        typedef T mapped_type;
        typedef typename hash_table_type::entry_type entry_type;

        /// iterator (not STL compliant)
        class iterator {
        // TODO: iterator interface is totally ugly!!!!
        public:
            iterator() noexcept
                : m_table(nullptr)
                , m_pos(std::numeric_limits<size_type>::max()) {
            }

            iterator(const iterator &) noexcept = default;

            iterator & operator= (const iterator &) = default;

            explicit operator bool() const noexcept {
                return m_table != nullptr && not m_table->empty_at(m_pos);
            }

            const key_type key() const noexcept {
                if (*this) {
                    return m_table->entry_at(m_pos).key();
                } else {
                    return Key();
                }
            }

            const mapped_type value() const noexcept {
                if (*this) {
                    return m_table->entry_at(m_pos).value();
                } else {
                    return T();
                }
            }

            void unsafe_replace_kv(const key_type k, const hash_type h, mapped_type v) noexcept {
                debug_assert(*this);
                entry_type new_entry(k, v);
                debug_assert(m_table->hash_at(m_pos) == h); (void)h;
                m_table->entry_at(m_pos).swap(new_entry);
            }

        private:
            typedef typename hash_table_type::size_type size_type;
            friend class dict<Key, T, KeyEqual, Entry, Options>;
            iterator(hash_table_type * t, size_type p) noexcept : m_table(t), m_pos(p) {}
            hash_table_type * m_table;
            size_type m_pos;
        };

    private:
        static constexpr size_type default_initial_size = 16;
        typedef typename hash_table_type::entry_type entry;

    public:
        /// constructor
        dict(const size_type initial_size = default_initial_size)
            : m_primary_tbl(new hash_table_type(roundup_pow2(initial_size)))
            , m_secondary_tbl(nullptr)
            , m_hashpower(log2u(roundup_pow2(initial_size)))
            , m_expand_pos(0)
        {
            debug_assert(initial_size > 0);
            if (not m_primary_tbl->ok()) {
                throw std::bad_alloc();
            }
            debug_assert(pow2(m_hashpower) == m_primary_tbl->capacity());
        }

        // disallow copying
        dict(const dict &) = delete;
        dict & operator= (const dict &) = delete;

        /// @copydoc hash_table::get
        tuple<bool, mapped_type> get(const key_type key, const hash_type hash) const noexcept {
            if (not is_expanding()) {
                return m_primary_tbl->get(key, hash);
            } else {
                tuple<bool, mapped_type> result = m_secondary_tbl->get(key, hash);
                bool found = std::get<0>(result);
                if (found) {
                    return result;
                } else {
                    return m_primary_tbl->get(key, hash);
                }
            }
        }

        /// return either iterator referencing existing entry or pointer to insertion position
        tuple<bool, iterator> entry_for(key_type key, hash_type hash, bool readonly = false) {
            if (not is_expanding()) {
                return search_primary(key, hash, readonly);
            } else {
                // dict expansion is in progress
                return search_secondary(key, hash);
            }
        }

        /// insert new item at position obtained from entry_for() call
        iterator insert(iterator where, key_type key, hash_type hash, mapped_type value) noexcept {
            hash_table_type * table = where.m_table;
            debug_assert(raw_pointer(m_primary_tbl) == table ||  raw_pointer(m_secondary_tbl) == table);
            debug_assert(not table->contains(key, hash));
            return iterator(table, table->insert(where.m_pos, key, hash, value));
        }

        /// @copydoc hash_table::del
        bool del(key_type key, hash_type hash) noexcept {
            if (not is_expanding()) {
                return m_primary_tbl->del(key, hash);
            } else {
                bool deleted = m_secondary_tbl->del(key, hash);
                if (not deleted) {
                    deleted = m_primary_tbl->del(key, hash);
                }
                rehash_some();
                return deleted;
            }
        }

        /// @copydoc hash_table::remove
        void remove(iterator where) noexcept {
            hash_table_type * table = where.m_table;
            debug_assert(raw_pointer(m_primary_tbl) == table ||  raw_pointer(m_secondary_tbl) == table);
            return table->remove(where.m_pos);
        }


        /// @copydoc hash_table::remove_if
        template <typename ConditionFun>
        void remove_if(ConditionFun predicate) noexcept {
            if (m_secondary_tbl) {
                m_secondary_tbl->remove_if(predicate);
            }
            m_primary_tbl->remove_if(predicate);
        }

        /// @copydoc hash_table::contains
        bool contains(key_type key, hash_type hash) const noexcept {
            if (not is_expanding()) {
                return m_primary_tbl->contains(key, hash);
            } else {
                return m_primary_tbl->contains(key, hash) || m_secondary_tbl->contains(key, hash);
            }
        }

        /// capacity of dict
        size_type capacity() const noexcept {
            return m_primary_tbl->capacity();
        }

        /// number of stored items
        size_type size() const noexcept {
            size_type num_elements = m_primary_tbl->size();
            if (is_expanding()) {
                num_elements += m_secondary_tbl->size();
            }
            debug_assert(num_elements <= capacity());
            return num_elements;
        }

        /// check whether dict is empty
        bool empty() const noexcept {
            return size() == 0;
        }

        /// indicates that dict is in progress of moving items to the new hash_table
        bool is_expanding() const noexcept { return m_secondary_tbl != nullptr; }

        /// empty the dictionary
        void clear() noexcept {
            m_secondary_tbl.reset(nullptr);
            m_primary_tbl->clear();
        }

    private:
        static iterator iter(std::unique_ptr<hash_table_type> & table, size_type pos_in_table) {
            return iterator(raw_pointer(table), pos_in_table);
        }

        tuple<bool, iterator> search_primary(key_type key, hash_type hash, bool readonly) {
            bool found; size_t at;
            tie(found, at) = m_primary_tbl->entry_for(key, hash);
            if (not found) {
                if (m_primary_tbl->threshold_reached()) {
                    if (not readonly) {
                        begin_expand();
                    }
                    // search for free entry again after resize
                    tie(found, at) = m_primary_tbl->entry_for(key, hash);
                    debug_assert(not found);
                }
            }
            return make_tuple(found, iter(m_primary_tbl, at)); // TODO: weak_ptr?
        }

        tuple<bool, iterator> search_secondary(key_type key, hash_type hash) noexcept {
            rehash_some();
            if (is_expanding()) {  // are we still expanding after rehash
                bool found; size_t old_pos;
                // lookup in secondary table first
                tie(found, old_pos) = m_secondary_tbl->entry_for(key, hash);
                if (found) {
                    // move item to the primary table and return its position there
                    bool __; size_t new_pos;
                    tie(__, new_pos) = m_primary_tbl->entry_for(key, hash);
                    debug_assert(not __); // not found in primary as it is in secondary already
                    const entry_type & e = m_secondary_tbl->entry_at(old_pos);
                    new_pos = m_primary_tbl->insert(new_pos, e.key(), hash, e.value());
                    m_secondary_tbl->remove(old_pos);
                    return make_tuple(true, iter(m_primary_tbl, new_pos));
                }
            }
            bool found; size_t at;
            tie(found, at) = m_primary_tbl->entry_for(key, hash);
            return make_tuple(found, iter(m_primary_tbl, at));
        }

        void begin_expand() {
            debug_assert(not is_expanding());
            m_expand_pos = 0;
            m_primary_tbl.swap(m_secondary_tbl);
            m_primary_tbl.reset(new hash_table_type(pow2(m_hashpower + 1)));
            if (m_primary_tbl && m_primary_tbl->ok()) {
                m_hashpower += 1;
                rehash_some();
            } else {
                m_primary_tbl.swap(m_secondary_tbl);
                m_secondary_tbl.reset(nullptr);
                throw std::bad_alloc();
            }
        }

        void end_expand() noexcept {
            debug_assert(is_expanding());
            debug_assert(m_secondary_tbl->empty());
            m_secondary_tbl.reset(nullptr);
            m_expand_pos = 0;
        }

        void rehash_some() noexcept {
            static constexpr size_type min_batch_size = 512;
            const size_type batch_size = std::min<size_type>(min_batch_size, m_secondary_tbl->size());
            size_type elements_moved = 0;
            while (elements_moved < batch_size) {
                while (m_secondary_tbl->empty_at(m_expand_pos)) {
                    m_expand_pos += 1;
                }
                debug_assert(m_expand_pos < m_secondary_tbl->capacity());
                const hash_table_type * old = m_secondary_tbl.get();
                hash_type hash = old->hash_at(m_expand_pos);
                const entry_type & e = old->entry_at(m_expand_pos);
                debug_assert(not m_primary_tbl->contains(e.key(), hash));
                m_primary_tbl->put(e.key(), hash, e.value());
                m_secondary_tbl->remove(m_expand_pos);
                elements_moved += 1;
            }
            if (m_secondary_tbl->empty()) {
                end_expand();
            }
        }

    private:
        std::unique_ptr<hash_table_type> m_primary_tbl;
        std::unique_ptr<hash_table_type> m_secondary_tbl;
        size_type m_hashpower;  // power of 2
        size_type m_expand_pos; // index of last element moved from secondary table to the primary
    };

} // namespace cachelot

#endif // CACHELOT_DICT_H_INCLUDED

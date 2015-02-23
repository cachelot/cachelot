#ifndef CACHELOT_CACHE_H_INCLUDED
#define CACHELOT_CACHE_H_INCLUDED

#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#  include <cachelot/memalloc.h>
#endif
#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#  include <cachelot/item.h>
#endif
#ifndef CACHELOT_DICT_H_INCLUDED
#  include <cachelot/dict.h>
#endif
#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_SETTINGS_H_INCLUDED
#  include <cachelot/settings.h>
#endif

/// @defgroup cache Cache implementation
/// @{

namespace cachelot {

    /// @ref cache
    namespace cache {

        /// Pointer to single cache Item
        typedef Item * ItemPtr;
        /// Hash value type
        typedef Item::hash_type hash_type;
        /// Clock to maintain expiration
        typedef Item::clock clock;
        /// Expiration time point
        typedef Item::expiration_time_point expiration_time_point;
        /// User defined flags
        typedef Item::opaque_flags_type opaque_flags_type;
        /// Value type of CAS operation
        typedef Item::cas_value_type cas_value_type;
        /// Maximum key length in bytes
        static constexpr auto max_key_length = Item::max_key_length;
        /// Maximum value length in bytes
        static constexpr auto max_value_length = Item::max_value_length;

        /// Single entry in the main cache dict
        class ItemDictEntry {
        public:
            ItemDictEntry() = default;
            explicit constexpr ItemDictEntry(const bytes &, const ItemPtr & the_item) noexcept : m_item(the_item) {}
            constexpr ItemDictEntry(ItemDictEntry &&) noexcept = default;
            ItemDictEntry & operator=(ItemDictEntry &&) noexcept = default;

            // disallow copying
            ItemDictEntry(const ItemDictEntry &) = delete;
            ItemDictEntry & operator=(const ItemDictEntry &) = delete;

            // key getter
            const bytes key() const noexcept {
                debug_assert(m_item);
                return m_item->key();
            }

            // value getter
            const ItemPtr & value() const noexcept {
                debug_assert(m_item);
                return m_item;
            }

            // swap with other entry
            void swap(ItemDictEntry & other) {
                using std::swap;
                swap(m_item, other.m_item);
            }
        private:
            ItemPtr m_item;
        };


        inline void swap(ItemDictEntry & left, ItemDictEntry & right) {
            left.swap(right);
        }

        /// Options of the underlying hash table
        struct DictOptions {
            typedef uint32 size_type;
            typedef hash_type hash_type;
            static constexpr size_type max_load_factor_percent = 93;
        };


        /**
         * Cache class to rule them all
         *
         * @note Cache is *not* thread safe
         */
        class Cache {
           // Underlying dictionary
            typedef dict<bytes, ItemPtr, std::equal_to<bytes>, ItemDictEntry, DictOptions> dict_type;
            typedef dict_type::iterator iterator;
        public:
            typedef dict_type::hash_type hash_type;
            typedef dict_type::size_type size_type;
        public:
            /**
             * constructor
             *
             * @param memory_size - amount of memory available for storage use
             * @param initial_dict_size - number of reserved items in dictionary
             */
            explicit Cache(size_t memory_size, size_t initial_dict_size);


            /**
             * `get` -  retrieve item
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_get(error_code error, bool found, bytes value, opaque_flags_type flags, cas_value_type cas_value)
             * @endcode
             */
            template <typename Callback>
            void do_get(const bytes key, const hash_type hash, Callback on_get) noexcept;


            /**
             * @class doxygen_store_command
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_complete(error_code error, bool stored)
             * @endcode
             */

            /**
             * `set` - store item unconditionally
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_set(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_set) noexcept;

            /**
             * `add` - store non-existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_add(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_add) noexcept;

            /**
             * `replace` - modify existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_replace(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_replace) noexcept;

            /**
             * `cas` - compare-and-swap items
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_cas(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_cas) noexcept;

            /**
             * `append` - write additional data 'after' existing item data
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_append(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_append) noexcept;

            /**
             * `prepend` - write additional data 'before' existing item data
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void do_prepend(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_prepend) noexcept;

            /**
             * `del` - delete existing item
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_del(error_code error, bool deleted)
             * @endcode
             */
            template <typename Callback>
            void do_del(const bytes key, const hash_type hash, Callback on_del) noexcept;

            /**
             * `touch` - prolong item lifetime
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_touch(error_code error, bool touched)
             * @endcode
             */
            template <typename Callback>
            void do_touch(const bytes key, const hash_type hash, seconds expires, Callback on_touch) noexcept;

        private:
            /**
             * utility function to create point in time from duration in seconds
             */
            static expiration_time_point time_from(seconds expire_after) {
                // TODO: !Important check that duration will not overflow
                //if (std::numeric_limits<seconds::rep>::max() - expire_after.count() )
                return expire_after == seconds(0) ? expiration_time_point::max() : clock::now() + expire_after;
            }

            /**
             * Create new Item using memalloc for allocation
             * throws std::bad_alloc on allocation failure
             */
            ItemPtr item_new(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value);

            /**
             * Free existing Item and return memory to the memalloc
             */
            void item_free(ItemPtr item) noexcept;

            /**
             * Try to assign new fields to the existing Item in dict at given position
             * throws std::bad_alloc on allocation failure
             */
            void item_reassign_at(const iterator at, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value);

            /**
             * retrieve item from cache taking in account its expiration time,
             * expired items will be immediately removed and retrieve_item() will report that item was not found
             */
            tuple<bool, dict_type::iterator> retrieve_item(const bytes key, const hash_type hash, bool readonly = false);

        private:
            std::unique_ptr<uint8[]> memory_arena;
            memalloc m_allocator;
            dict_type m_dict;
        };


        inline Cache::Cache(size_t memory_size, size_t initial_dict_size)
            : memory_arena(new uint8[memory_size])
            , m_allocator(raw_pointer(memory_arena), memory_size)
            , m_dict(initial_dict_size) {
        }


        inline tuple<bool, Cache::dict_type::iterator> Cache::retrieve_item(const bytes key, const hash_type hash, bool readonly) {
            bool found; iterator at;
            tie(found, at) = m_dict.entry_for(key, hash, readonly);
            if (found && at.value()->is_expired()) {
                Item * item = at.value();
                m_dict.remove(at);
                item_free(item);
                found = false;
            }
            return make_tuple(found, at);
        }


        template <typename Callback>
        inline void Cache::do_get(const bytes key, const hash_type hash, Callback on_get) noexcept {
            // try to retrieve existing item
            bool found; iterator at; bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                auto item = at.value();
                debug_assert(item->key() == key);
                debug_assert(item->hash() == hash);
                on_get(error::success, true, item->value(), item->opaque_flags(), item->cas_value());
            } else {
                on_get(error::success, false, bytes(), opaque_flags_type(), cas_value_type());
            }
        }


        template <typename Callback>
        inline void Cache::do_set(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_set) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(key, hash);
                if (not found) {
                    auto item = item_new(key, hash, value, flags, expires, cas_value);
                    m_dict.insert(at, key, hash, item);
                } else {
                    item_reassign_at(at, value, flags, expires, cas_value);
                }
                on_set(error::success, true);
            } catch (const std::bad_alloc &) {
                on_set(error::out_of_memory, false);
            }
        }


        template <typename Callback>
        inline void Cache::do_add(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_add) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(key, hash);
                if (not found) {
                    auto item = item_new(key, hash, value, flags, expires, cas_value);
                    m_dict.insert(at, key, hash, item);
                }
                on_add(error::success, not found);
            } catch (const std::bad_alloc &) {
                on_add(error::out_of_memory, false);
            }
        }


        template <typename Callback>
        inline void Cache::do_replace(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_replace) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(key, hash);
                if (found) {
                    item_reassign_at(at, value, flags, expires, cas_value);
                }
                on_replace(error::success, found);
            } catch (const std::bad_alloc &) {
                on_replace(error::out_of_memory, false);
            }
        }


        template <typename Callback>
        inline void Cache::do_cas(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_cas) noexcept {
            (void)key; (void)hash; (void)value; (void)flags; (void)expires; (void)cas_value;
        }


        template <typename Callback>
        inline void Cache::do_append(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_append) noexcept {
            (void)key; (void)hash; (void)value; (void)flags; (void)expires; (void)cas_value;
        }


        template <typename Callback>
        inline void Cache::do_prepend(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_prepend) noexcept {
            (void)key; (void)hash; (void)value; (void)flags; (void)expires; (void)cas_value;
        }


        template <typename Callback>
        inline void Cache::do_del(const bytes key, const hash_type hash, Callback on_del) noexcept {
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                Item * item = at.value();
                m_dict.remove(at);
                item_free(item);
            }
            on_del(error::success, found);
        }


        template <typename Callback>
        inline void Cache::do_touch(const bytes key, const hash_type hash, seconds expires, Callback on_touch) noexcept {
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                at.value()->touch(time_from(expires));
            }
            on_touch(error::success, found);
        }


        inline ItemPtr Cache::item_new(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value) {
            void * memory;
            const size_t size_required = Item::CalcSizeRequired(key, value, cas_value);
            static const auto on_delete = [=](void * ptr) -> void {
                auto i = reinterpret_cast<Item *>(ptr);
                debug_only(bool deleted = ) this->m_dict.del(i->key(), i->hash());
                debug_assert(deleted);
            };
            memory = m_allocator.alloc_or_evict(size_required, settings.cache.has_evictions, on_delete);
            if (memory != nullptr) {
                return new (memory) Item(key, hash, value, flags, time_from(expires), cas_value);
            } else {
                throw std::bad_alloc();
            }
        }


        inline void Cache::item_free(ItemPtr item) noexcept {
            debug_assert(not m_dict.contains(item->key(), item->hash()));
            m_allocator.free(item);
        }


        inline void Cache::item_reassign_at(const iterator at, bytes new_value, opaque_flags_type new_flags, seconds new_expires, cas_value_type new_cas_value) {
            Item * item = at.value();
            const size_t size_required = Item::CalcSizeRequired(item->key(), new_value, new_cas_value);
            // Try to resize Item's memory without touching contents
            if (m_allocator.try_realloc_inplace(item, size_required)) {
                item->reassign(new_value, new_flags, time_from(new_expires), new_cas_value);
            } else {
                auto new_item = item_new(item->key(), item->hash(), new_value, new_flags, new_expires, new_cas_value);
                // TODO: we may double delete same Item (previously during eviction)
                m_dict.remove(at);
                item_free(item);
                m_dict.insert(at, new_item->key(), new_item->hash(), new_item);
            }
        }



    } // namespace cache

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_H_INCLUDED

#ifndef CACHELOT_CACHE_H_INCLUDED
#define CACHELOT_CACHE_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_MEMALLOC_H_INCLUDED
#  include <cachelot/memalloc.h>
#endif
#ifndef CACHELOT_CACHE_ITEM_H_INCLUDED
#  include <cachelot/item.h>
#endif
#ifndef CACHELOT_DICT_H_INCLUDED
#  include <cachelot/dict.h>
#endif
#ifndef CACHELOT_HASH_FNV1A_H_INCLUDED
#  include <cachelot/hash_fnv1a.h>
#endif
#ifndef CACHELOT_ERROR_H_INCLUDED
#  include <cachelot/error.h>
#endif
#ifndef CACHELOT_STRING_CONV_H_INCLUDED
#  include <cachelot/string_conv.h>
#endif
#ifndef CACHELOT_STATS_H_INCLUDED
#  include <cachelot/stats.h>
#endif

namespace cachelot {

    /// @defgroup cache Cache implementation
    /// @{

    namespace cache {
        /// Pointer to single cache Item
        typedef Item * ItemPtr;
        typedef const Item * ConstItemPtr;

        /// Hash value type
        typedef Item::hash_type hash_type;

        /// Hashing algorithm
        typedef fnv1a<cache::hash_type>::hasher HashFunction;

        /// Clock to maintain expiration
        typedef Item::clock clock;

        /// Expiration time point
        typedef Item::expiration_time_point expiration_time_point;

        /// User defined flags
        typedef Item::opaque_flags_type opaque_flags_type;

        /// Value type of CAS operation
        typedef Item::timestamp_type timestamp_type;

        /// Maximum key length in bytes
        static constexpr auto max_key_length = Item::max_key_length;

        /**
         * Item has both - the key and the value. This is wrapper to use Item pointer as a dict entry
         * @ingroup cache
         */
        class ItemDictEntry {
        public:
            ItemDictEntry() = default;
            explicit constexpr ItemDictEntry(const slice &, const ItemPtr & the_item) noexcept : m_item(the_item) {}
            constexpr ItemDictEntry(ItemDictEntry &&) noexcept = default;
            ItemDictEntry & operator=(ItemDictEntry &&) noexcept = default;

            // disallow copying
            ItemDictEntry(const ItemDictEntry &) = delete;
            ItemDictEntry & operator=(const ItemDictEntry &) = delete;

            // key getter
            const slice key() const noexcept {
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

        /**
         * Options of the underlying hash table
         * @ingroup cache
         */
        struct DictOptions {
            typedef uint32 size_type;
            typedef ::cachelot::cache::hash_type hash_type;
            static constexpr size_type max_load_factor_percent = 93;
        };


        /**
         * One cache class to rule them all
         *
         * @note Cache automaticaly manages lifetime of the stored items.
         * But in cases when Item was *not stored* after call, *user is responsible for deallocating* Item by himself.
         * Item may not be stored due to an error (the most likely out of memory) or due to invariant
         * Operations like add/replace/cas/inc/dec may not store item because conditions were not met
         * @see create_item() destroy_item()
         * @note Cache is *not* thread safe
         * @ingroup cache
         */
        class Cache {
           // Underlying dictionary
            typedef dict<slice, ItemPtr, std::equal_to<slice>, ItemDictEntry, DictOptions> dict_type;
            typedef dict_type::iterator iterator;
            /// INC/DEC
            enum class ArithmeticOperation { INCR, DECR };
            /// APPEND/PREPEND
            enum class ExtendOperation { APPEND, PREPEND };

            // Private constructor
            explicit Cache(size_t memory_limit, uint32 mem_page_size, dict_type::size_type initial_dict_size, bool enable_evictions);
        public:
            typedef dict_type::hash_type hash_type;
            typedef dict_type::size_type size_type;
        public:
            /**
             * static constructor function
             *
             * @param memory_limit - amount of memory available for storage use
             * @param mem_page_size - size of the allocator memory page
             * @param initial_dict_size - number of reserved items in dictionary
             * @param enable_evictions - evict existing items in order to store new ones
             * @note may throw exception
             */
            static Cache Create(size_t memory_limit, size_t mem_page_size, size_t initial_dict_size, bool enable_evictions);


            /**
             * destructor
             */
            ~Cache();


            /**
             * Move constructor
             */
            Cache(Cache && c) = default;


            /**
             * `get` -  retrieve item
             *
             * @return pointer to the Item or `nullptr` if none was found
             * @warning pointer is only valid *until* the next cache API call
             */
            ConstItemPtr do_get(const slice key, const hash_type hash) noexcept;

            /**
             * `set` - store item unconditionally
             *
             */
            void do_set(ItemPtr item);

            /**
             * `add` - store non-existing item
             *
             * @return
             * - `true` - item was stored
             * - `false` - key already exists
             */
            bool do_add(ItemPtr item);

            /**
             * `replace` - modify existing item
             *
             * @return
             * - `true` - item was stored
             * - `false` - no such key
             */
            bool do_replace(ItemPtr item);

            /**
             * `cas` - compare-and-swap items
             *
             * @return tuple<found, stored>
             * - `[true, true]` - item was updated
             * - `[true, false]` - item was not updated as it has been modified since
             * - `[false, false]` - no such key
             */
            tuple<bool, bool> do_cas(ItemPtr item, timestamp_type cas_unique);

            /**
             * `append` - append the data of the existing item
             *
             * @return
             * - `true` - item was stored
             * - `false` - no such key
             */
            bool do_append(ItemPtr item) { return do_extend(ExtendOperation::APPEND, item); }

            /**
             * `prepend` - prepend the data of the existing item
             *
             * @return
             * - `true` - item was stored
             * - `false` - no such key
             */
            bool do_prepend(ItemPtr item) { return do_extend(ExtendOperation::PREPEND, item); }

            /**
             * `delete` - delete existing item
             *
             * @return
             * - `true` - item was deleted
             * - `false` - no such key
             */
            bool do_delete(const slice key, const hash_type hash) noexcept;

            /**
             * `touch` - validate item and prolong its lifetime
             *
             * @return
             * - `true` - item TTL was updated
             * - `false` - no such key
             */
            bool do_touch(const slice key, const hash_type hash, seconds keepalive) noexcept;

            /**
             * `flush_all` - invalidate every item in the cache (remove expired items)
             */
            void do_flush_all() noexcept;

            /**
             * `incr` - increment counter by its `key`
             *
             * value is taken as ASCII encoded unsigned 64-bit integer
             * If overflow happens, value will be set to int64_max
             *
             * @return tuple<found, new_value>
             */
            tuple<bool, uint64> do_incr(const slice key, const hash_type hash, uint64 delta) {
                return do_arithmetic(ArithmeticOperation::INCR, key, hash, delta);
            }

            /**
             * `decr` - decrement counter by its `key`
             *
             * value is taken as ASCII encoded unsigned 64-bit integer
             * Decrement operation handle underflow and set value to zero if `delta` is greater than item value
             *
             * @return tuple<found, new_value>
             */
            tuple<bool, uint64> do_decr(const slice key, const hash_type hash, uint64 delta) {
                return do_arithmetic(ArithmeticOperation::DECR, key, hash, delta);
            }

            /**
             * Create new Item from the pre-allocated memory arena
             */
            ItemPtr create_item(const slice key, const hash_type hash, size_t value_length, opaque_flags_type flags, seconds keepalive);

            /**
             * Free existing Item and return the memory
             */
            void destroy_item(ItemPtr item) noexcept;

            /**
             * Publish dynamic stats
             */
            void publish_stats() noexcept;

        private:
            /**
             * Extend (`prepend` or `append`) existing item with the new data
             *
             * @return
             * - `true` - item was stored
             * - `false` - no such key
             */
            bool do_extend(ExtendOperation op, ItemPtr item);

            /**
             * Make one of the ArithmeticOperation
             *
             * @return tuple<found, new_value>
             */
            tuple<bool, uint64> do_arithmetic(ArithmeticOperation op, const slice key, const hash_type hash, uint64 delta);

            /**
             * Retrieve item from cache taking in account its expiration time;
             * expired items will be immediately removed and retrieve_item() will report that item was not found
             */
            tuple<bool, dict_type::iterator> retrieve_item(const slice key, const hash_type hash, bool readonly = false);

            class ItemAutoDelete {
                Cache * m_cache;
                Item * m_item;
            public:
                explicit ItemAutoDelete(Cache * c, Item * i) noexcept : m_cache(c), m_item(i) {}
                ~ItemAutoDelete() { if (m_item) m_cache->destroy_item(m_item); }
                void reset() noexcept { m_item = nullptr; }
                ItemPtr get() noexcept { return m_item; }
            };

            /**
             * Replace Item at position with another one
             */
            void replace_item_at(const iterator at, ItemAutoDelete & lockedItem) noexcept;

            /**
             * Add new item to the cache
             */
            void insert_item_at(const iterator at, ItemAutoDelete & lockedItem) noexcept;

        private:
            memalloc m_allocator;
            dict_type m_dict;
            const bool m_evictions_enabled;
            timestamp_type m_oldest_timestamp;
            timestamp_type m_newest_timestamp;
        };


        inline Cache Cache::Create(size_t memory_limit, size_t mem_page_size, size_t initial_dict_size, bool enable_evictions) {
            if (not ispow2(memory_limit)) {
                throw std::invalid_argument("memory_limit must be power of 2");
            }
            if (memory_limit < (mem_page_size * 4)) {
                throw std::invalid_argument("memory_limit should be enough for at least 4 pages");
            }
            if (not ispow2(mem_page_size)) {
                throw std::invalid_argument("mem_page_size must be power of 2");
            }
            if (mem_page_size > std::numeric_limits<uint32>::max()) {
                throw std::invalid_argument("mem_page_size is too big");
            }
            if (not ispow2(initial_dict_size)) {
                throw std::invalid_argument("initial_dict_size must be power of 2");
            }
            if (initial_dict_size > std::numeric_limits<dict_type::size_type>::max()) {
                throw std::invalid_argument("initial_dict_size is too big");
            }
            if (memory_limit % mem_page_size != 0) {
                throw std::invalid_argument("memory_limit divide by mem_page_size should be integer");
            }
            return Cache(memory_limit, mem_page_size, initial_dict_size, enable_evictions);
        }


        inline Cache::Cache(size_t memory_limit, uint32 mem_page_size, dict_type::size_type initial_dict_size, bool enable_evictions)
            : m_allocator(memory_limit, mem_page_size)
            , m_dict(initial_dict_size)
            , m_evictions_enabled(enable_evictions)
            , m_oldest_timestamp(std::numeric_limits<timestamp_type>::max())
            , m_newest_timestamp(std::numeric_limits<timestamp_type>::min()) {
        }


        inline Cache::~Cache() {
            m_dict.remove_if([=](ItemPtr item) -> bool {
                destroy_item(item);
                return true;
            });
        }


        inline tuple<bool, Cache::dict_type::iterator> Cache::retrieve_item(const slice key, const hash_type hash, bool readonly) {
            bool found; iterator at;
            tie(found, at) = m_dict.entry_for(key, hash, readonly);
            if (found) {
                ItemPtr item = at.value();
                // validate item
                if (not item->is_expired()) {
                    m_allocator.touch(item);
                } else {
                    m_dict.remove(at);
                    destroy_item(item);
                    found = false;
                }
            }
            return make_tuple(found, at);
        }


        inline ConstItemPtr Cache::do_get(const slice key, const hash_type hash) noexcept {
            STAT_INCR(cache.cmd_get, 1);
            // try to retrieve existing item
            bool found; iterator at; bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                STAT_INCR(cache.get_hits, 1);
                auto item = at.value();
                debug_assert(item->key() == key);
                debug_assert(item->hash() == hash);
                return item;
            } else {
                STAT_INCR(cache.get_misses, 1);
                return ItemPtr(nullptr);
            }
        }


        inline void Cache::do_set(ItemPtr item) {
            STAT_INCR(cache.cmd_set, 1);
            ItemAutoDelete _item_uniq_ptr(this, item);
            bool found; iterator at;
            tie(found, at) = retrieve_item(item->key(), item->hash());
            if (not found) {
                STAT_INCR(cache.set_new, 1);
                insert_item_at(at, _item_uniq_ptr);
            } else {
                STAT_INCR(cache.set_existing, 1);
                replace_item_at(at, _item_uniq_ptr);
            }
        }


        inline bool Cache::do_add(ItemPtr item) {
            STAT_INCR(cache.cmd_add, 1);
            ItemAutoDelete _item_uniq_ptr(this, item);
            bool found; iterator at;
            tie(found, at) = retrieve_item(item->key(), item->hash());
            if (not found) {
                insert_item_at(at, _item_uniq_ptr);
                STAT_INCR(cache.add_stored, 1);
                return true;
            } else {
                STAT_INCR(cache.add_not_stored, 1);
                return false;
            }
        }


        inline bool Cache::do_replace(ItemPtr item) {
            STAT_INCR(cache.cmd_replace, 1);
            ItemAutoDelete _item_uniq_ptr(this, item);
            bool found; iterator at;
            tie(found, at) = retrieve_item(item->key(), item->hash());
            if (found) {
                replace_item_at(at, _item_uniq_ptr);
                STAT_INCR(cache.replace_stored, 1);
                return true;
            } else {
                STAT_INCR(cache.replace_not_stored, 1);
                return false;
            }
        }


        inline tuple<bool, bool> Cache::do_cas(ItemPtr item, timestamp_type cas_unique) {
            STAT_INCR(cache.cmd_cas, 1);
            ItemAutoDelete _item_uniq_ptr(this, item);
            bool found; iterator at;
            tie(found, at) = retrieve_item(item->key(), item->hash());
            if (found) {
                if (cas_unique == at.value()->timestamp()) {
                    replace_item_at(at, _item_uniq_ptr);
                    STAT_INCR(cache.cas_stored, 1);
                    return make_tuple(true, true);
                } else {
                    STAT_INCR(cache.cas_badval, 1);
                    return make_tuple(true, false);
                }
            } else {
                STAT_INCR(cache.cas_misses, 1);
                return make_tuple(false, false);
            }
        }


        inline bool Cache::do_extend(ExtendOperation op, ItemPtr piece) {
            if (op == ExtendOperation::APPEND) {
                STAT_INCR(cache.cmd_append, 1);
            } else {
                debug_assert(op == ExtendOperation::PREPEND);
                STAT_INCR(cache.cmd_prepend, 1);
            }
            ItemAutoDelete _piece_unique_ptr(this, piece);
            bool found; iterator at;
            tie(found, at) = retrieve_item(piece->key(), piece->hash());
            if (found) {
                auto old_item = at.value();
                const size_t new_value_size = old_item->value().length() + piece->value().length();
                // do not evict existing items to avoid accidentally free the `piece` or the `old_item`
                auto memory = m_allocator.alloc_or_evict(Item::CalcSizeRequired(old_item->key(), new_value_size), false, [=](void *){});
                if (memory != nullptr) {
                    auto new_item = new (memory) Item(old_item->key(), old_item->hash(), static_cast<uint32>(new_value_size), old_item->opaque_flags(), old_item->ttl(), ++m_newest_timestamp);
                    ItemAutoDelete _item_uniq_ptr(this, new_item);
                    if (op == ExtendOperation::APPEND) {
                        new_item->assign_compose(old_item->value(), piece->value());
                        STAT_INCR(cache.append_stored, 1);
                    } else {
                        debug_assert(op == ExtendOperation::PREPEND);
                        new_item->assign_compose(piece->value(), old_item->value());
                        STAT_INCR(cache.prepend_stored, 1);
                    }
                    replace_item_at(at, _item_uniq_ptr);
                } else {
                    throw system_error(error::out_of_memory);
                }
            } else {
                if (op == ExtendOperation::APPEND) {
                    STAT_INCR(cache.append_misses, 1);
                } else {
                    debug_assert(op == ExtendOperation::PREPEND);
                    STAT_INCR(cache.prepend_misses, 1);
                }
            }
            return found;
        }


        inline bool Cache::do_delete(const slice key, const hash_type hash) noexcept {
            STAT_INCR(cache.cmd_delete, 1);
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                ItemPtr item = at.value();
                m_dict.remove(at);
                destroy_item(item);
                STAT_INCR(cache.delete_hits, 1);
                return true;
            } else {
                STAT_INCR(cache.delete_misses, 1);
                return false;
            }

        }


        inline bool Cache::do_touch(const slice key, const hash_type hash, seconds keepalive) noexcept {
            STAT_INCR(cache.cmd_touch, 1);
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                auto item = at.value();
                m_allocator.touch(item); // mark item as recent in LRU list
                item->set_ttl(keepalive); // update lifetime
                STAT_INCR(cache.touch_hits, 1);
                return true;
            } else {
                STAT_INCR(cache.touch_misses, 1);
                return false;
            }
        }


        inline void Cache::do_flush_all() noexcept {
            STAT_INCR(cache.cmd_flush, 1);
            m_dict.remove_if([=](ItemPtr item) -> bool {
                if (item->is_expired()) {
                    destroy_item(item);
                    return true;
                } else {
                    return false;
                }
            });
        }


        inline tuple<bool, uint64> Cache::do_arithmetic(ArithmeticOperation op, const slice key, const hash_type hash, uint64 delta) {
            if (op == ArithmeticOperation::INCR) {
                STAT_INCR(cache.cmd_incr, 1);
            } else {
                debug_assert(op == ArithmeticOperation::DECR);
                STAT_INCR(cache.cmd_decr, 1);
            }
            bool found; iterator at;
            tie(found, at) = retrieve_item(key, hash);
            if (not found) {
                if (op == ArithmeticOperation::INCR) {
                    STAT_INCR(cache.incr_misses, 1);
                } else {
                    debug_assert(op == ArithmeticOperation::DECR);
                    STAT_INCR(cache.decr_misses, 1);
                }
                return make_tuple(false, 0ull);
            }
            // retrieve item value stored as an ASCII string
            auto old_item = at.value();
            auto old_ascii_value = at.value()->value();
            auto old_int_value = str_to_int<uint64>(old_ascii_value.begin(), old_ascii_value.end());
            // process arithmetic command
            uint64 new_int_value = 0;
            if (op == ArithmeticOperation::INCR) {
                constexpr auto max = std::numeric_limits<uint64>::max();
                new_int_value = (max - old_int_value >= delta) ? old_int_value + delta : max;
                STAT_INCR(cache.incr_hits, 1);
            } else {
                debug_assert(op == ArithmeticOperation::DECR);
                new_int_value = (old_int_value >= delta) ? old_int_value - delta : 0;
                STAT_INCR(cache.decr_hits, 1);
            }
            // store new value as an ASCII string
            AsciiIntegerBuffer new_ascii_value;
            const auto new_ascii_value_length = int_to_str(new_int_value, new_ascii_value);
            // create new item to hold value including zero terminator
            ItemPtr new_item;
            new_item = create_item(old_item->key(), old_item->hash(), new_ascii_value_length, old_item->opaque_flags(), old_item->ttl());
            ItemAutoDelete _item_uniq_ptr(this, new_item);
            new_item->assign_value(slice(new_ascii_value, new_ascii_value_length));
            replace_item_at(at, _item_uniq_ptr);
            return make_tuple(true, new_int_value);
        }


        inline ItemPtr Cache::create_item(const slice key, const hash_type hash, size_t value_length, opaque_flags_type flags, seconds keepalive) {
            void * memory;
            if (key.length() > Item::max_key_length) {
                throw system_error(error::key_too_long);
            }
            const size_t size_required = Item::CalcSizeRequired(key, value_length);
            if (size_required > m_allocator.page_size) {
                throw system_error(error::item_too_big);
            }
            static const auto on_delete = [=](void * ptr) noexcept -> void {
                auto i = reinterpret_cast<Item *>(ptr);
                debug_only(bool deleted = ) this->m_dict.del(i->key(), i->hash());
                debug_assert(deleted);
            };
            memory = m_allocator.alloc_or_evict(size_required, m_evictions_enabled, on_delete);
            if (memory != nullptr) {
                auto item = new (memory) Item(key, hash, static_cast<uint32>(value_length), flags, keepalive, ++m_newest_timestamp);
                return item;
            } else {
                throw system_error(error::out_of_memory);
            }
        }


        inline void Cache::destroy_item(ItemPtr item) noexcept {
            m_allocator.free(item);
        }


        inline void Cache::replace_item_at(iterator at, ItemAutoDelete & lockedItem) noexcept {
            auto old_item = at.value();
            auto new_item = lockedItem.get();
            debug_assert(old_item->hash() == new_item->hash() && old_item->key() == new_item->key());
            destroy_item(old_item);
            at.unsafe_replace_kv(new_item->key(), new_item->hash(), new_item);
            lockedItem.reset(); // Item will live
        }


        inline void Cache::insert_item_at(const iterator at, ItemAutoDelete & lockedItem) noexcept {
            auto i = lockedItem.get();
            m_dict.insert(at, i->key(), i->hash(), i);
            lockedItem.reset();
        }


        inline void Cache::publish_stats() noexcept {
            STAT_SET(cache.hash_capacity, m_dict.capacity());
            STAT_SET(cache.curr_items, m_dict.size());
            STAT_SET(cache.hash_is_expanding, m_dict.is_expanding());
        }

    } // namespace cache

    /// @}

} // namespace cachelot


#endif // CACHELOT_CACHE_H_INCLUDED

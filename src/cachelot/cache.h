#ifndef CACHELOT_CACHE_H_INCLUDED
#define CACHELOT_CACHE_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_CACHE_DEFS_H_INCLUDED
#  include <cachelot/cache_defs.h>
#endif
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

        /// Hashing algorithm
        typedef fnv1a<cache::hash_type>::hasher HashFunction;

        /// Clock to maintain expiration
        typedef Item::clock clock;

        /// Expiration time point
        typedef Item::expiration_time_point expiration_time_point;

        /// User defined flags
        typedef Item::opaque_flags_type opaque_flags_type;

        /// Value type of CAS operation
        typedef Item::version_type version_type;

        /// Maximum key length in bytes
        static constexpr auto max_key_length = Item::max_key_length;

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
            typedef ::cachelot::cache::hash_type hash_type;
            static constexpr size_type max_load_factor_percent = 93;
        };


        /**
         * One cache class to rule them all
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
            explicit Cache();


            /**
             * `get` -  retrieve item
             *
             * @tparam Callback - callback will be called when request is completed
             *
             * Callback must have following signature:
             * @code
             *    void on_get(error_code error, bool found, bytes value, opaque_flags_type flags, version_type version)
             * @endcode
             * @warning Key/value pointers may not be valid outside of callback.
             */
            template <typename Callback>
            void do_get(const bytes key, const hash_type hash, Callback on_get) noexcept;


            /**
             * @class doxygen_store_command
             *
             * @return `tuple<error_code, Response>`
             *  - error: indicates that Item has *not* stored because of an error (the most likely out of memory)
             *  - Response: one of a possible cache responses
             * TODO: responses
             */


            /**
             * execute on of the storage command
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_storage(Command cmd, ItemPtr item) noexcept;

            /**
             * `set` - store item unconditionally
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_set(ItemPtr item) noexcept;

            /**
             * `add` - store non-existing item
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_add(ItemPtr item) noexcept;

            /**
             * `replace` - modify existing item
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_replace(ItemPtr item) noexcept;

            /**
             * `cas` - compare-and-swap items
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_cas(ItemPtr item) noexcept;

            /**
             * `append` - write additional data 'after' existing item data
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_append(ItemPtr item) noexcept;
            
            /**
             * `prepend` - write additional data 'before' existing item data
             *
             * @copydoc doxygen_store_command
             */
            tuple<error_code, Response> do_prepend(ItemPtr item) noexcept;

            /**
             * `del` - delete existing item
             */
            tuple<error_code, Response> do_del(const bytes key, const hash_type hash) noexcept;

            /**
             * `touch` - validate item and prolong its lifetime
             */
            tuple<error_code, Response> do_touch(const bytes key, const hash_type hash, seconds expires) noexcept;
            
            
            /**
             * `incr` or `decr` value by `delta`
             * On arithmetic operations value is treaten as an decimal ASCII encoded unsigned 64-bit integer
             * Decrement operation handle underflow and set value to zero if `delta` is greater than item value
             * Owerflow in `incr` command is hardware-dependent integer overflow
             */
            tuple<error_code, Response, uint64> do_arithmetic(Command cmd, const bytes key, const hash_type hash, uint64 delta) noexcept;


            /**
             * Create new Item using memalloc
             */
            tuple<error_code, ItemPtr> create_item(const bytes key, const hash_type hash, uint32 value_length, opaque_flags_type flags, expiration_time_point expiration, version_type ver) noexcept;

            /**
             * Allocate new Item as a new version of existing
             */
            tuple<error_code, ItemPtr> create_item_version(ItemPtr existing_item, uint32 new_item_value_length) noexcept;


            /**
             * Free existing Item and return memory to the memalloc
             */
            void item_free(ItemPtr item) noexcept;

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
             * Replace Item at position with another one
             */
            void replace_item_at(const iterator at, ItemPtr new_item) noexcept;

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


        inline Cache::Cache()
            : memory_arena(new uint8[settings.cache.memory_limit])
            , m_allocator(raw_pointer(memory_arena), settings.cache.memory_limit)
            , m_dict(settings.cache.initial_hash_table_size) {
        }


        inline tuple<bool, Cache::dict_type::iterator> Cache::retrieve_item(const bytes key, const hash_type hash, bool readonly) {
            bool found; iterator at;
            tie(found, at) = m_dict.entry_for(key, hash, readonly);
            if (found && at.value()->is_expired()) {
                ItemPtr item = at.value();
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
                on_get(error::success, true, item->value(), item->opaque_flags(), item->version());
            } else {
                on_get(error::success, false, bytes(), opaque_flags_type(), version_type());
            }
        }


        inline tuple<error_code, Response> Cache::do_storage(Command cmd, ItemPtr item) noexcept {
            switch (cmd) {
            case SET:
                return do_set(item);
            case ADD:
                return do_add(item);
            case REPLACE:
                return do_replace(item);
            case APPEND:
                return do_append(item);
            case PREPEND:
                return do_prepend(item);
            case CAS:
                return do_cas(item);
            default:
                debug_assert(false);
                return make_tuple(error::unknown_error, NOT_A_RESPONSE);
            }
        }



        inline tuple<error_code, Response> Cache::do_set(ItemPtr item) noexcept {
            try {
                bool found; iterator at;
                tie(found, at) = retrieve_item(item->key(), item->hash());
                if (not found) {
                    m_dict.insert(at, item->key(), item->hash(), item);
                } else {
                    replace_item_at(at, item);
                }
                return make_tuple(error::success, STORED);
            } catch (const std::bad_alloc &) {
                return make_tuple(error::out_of_memory, NOT_A_RESPONSE);
            }
        }


        inline tuple<error_code, Response> Cache::do_add(ItemPtr item) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(item->key(), item->hash());
                if (not found) {
                    m_dict.insert(at, item->key(), item->hash(), item);
                }
                return make_tuple(error::success, not found ? STORED : NOT_STORED);
            } catch (const std::bad_alloc &) {
                return make_tuple(error::out_of_memory, NOT_A_RESPONSE);
            }
        }


        inline tuple<error_code, Response> Cache::do_replace(ItemPtr item) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(item->key(), item->hash());
                if (found) {
                    replace_item_at(at, item);
                }
                return make_tuple(error::success, found ? STORED : NOT_STORED);
            } catch (const std::bad_alloc &) {
                return make_tuple(error::out_of_memory, NOT_A_RESPONSE);
            }
        }


        inline tuple<error_code, Response> Cache::do_cas(ItemPtr item) noexcept {
            bool found; iterator at;
            try {
                tie(found, at) = retrieve_item(item->key(), item->hash());
                if (found) {
                    if (item->version() == at.value()->version()) {
                        replace_item_at(at, item);
                        return make_tuple(error::success, STORED);
                    } else {
                        return make_tuple(error::success, EXISTS);
                    }
                } else {
                    return make_tuple(error::success, NOT_FOUND);
                }
            } catch (const std::bad_alloc &) {
                return make_tuple(error::out_of_memory, NOT_A_RESPONSE);
            }
        }


        inline tuple<error_code, Response> Cache::do_append(ItemPtr item) noexcept {
            return make_tuple(error::not_implemented, NOT_A_RESPONSE);
        }


        inline tuple<error_code, Response> Cache::do_prepend(ItemPtr item) noexcept {
            return make_tuple(error::not_implemented, NOT_A_RESPONSE);
        }


        inline tuple<error_code, Response> Cache::do_del(const bytes key, const hash_type hash) noexcept {
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                ItemPtr item = at.value();
                m_dict.remove(at);
                item_free(item);
            }
            return make_tuple(error::success, found ? DELETED : NOT_FOUND);
        }


        inline tuple<error_code, Response> Cache::do_touch(const bytes key, const hash_type hash, seconds expires) noexcept {
            bool found; iterator at; const bool readonly = true;
            tie(found, at) = retrieve_item(key, hash, readonly);
            if (found) {
                at.value()->touch(time_from(expires));
            }
            return make_tuple(error::success, found ? TOUCHED : NOT_FOUND);
        }


        inline tuple<error_code, Response, uint64> Cache::do_arithmetic(Command cmd, const bytes key, const hash_type hash, uint64 delta) noexcept {
            try {
                bool found; iterator at;
                tie(found, at) = retrieve_item(key, hash);
                if (not found) {
                    return make_tuple(error::success, NOT_FOUND, 0ull);
                }
                // retrieve item value stored as an ASCII string
                auto item = at.value();
                char * old_str_value; size_t old_str_value_len;
                tie(old_str_value, old_str_value_len) = item->mutable_value();
                // convert string to int
                auto old_value = str_to_int<uint64>(old_str_value, old_str_value_len);
                // process arithmetic command
                uint64 new_value = 0;
                if (cmd == INCR) {
                    new_value = old_value + delta;
                } else if (cmd == DECR) {
                    new_value = (old_value >= delta) ? old_value - delta : 0;
                } else {
                    debug_assert(false && "Unknown command");
                }
                // store new value as an ASCII string
                char * new_str_value;
                auto new_str_value_length = uint_ascii_length(new_value);
                if (old_str_value_len > new_str_value_length) {
                    // re-assign value inplace
                    new_str_value = old_str_value;
                } else {
                    // create new item to hold value including zero terminator
                    error_code error; ItemPtr new_item;
                    tie(error, new_item) = create_item_version(item, new_str_value_length + 1);
                    if (error) {
                        throw system_error(error);
                    }
                    size_t __;
                    tie(new_str_value, __) = new_item->mutable_value();
                    replace_item_at(at, new_item);
                }
                debug_only(auto verify_value_length = ) int_to_str(new_value, new_str_value);
                debug_assert(verify_value_length == new_str_value_length);
                new_str_value[new_str_value_length] = '\0';
                return make_tuple(error::success, STORED, new_value);
            } catch(const system_error & e) {
                return make_tuple(e.code(), NOT_A_RESPONSE, 0ull);
            } catch (const std::invalid_argument &) {
                return make_tuple(error::invalid_argument, NOT_A_RESPONSE, 0ull);
            } catch (const std::overflow_error &) {
                return make_tuple(error::number_overflow, NOT_A_RESPONSE, 0ull);
            } catch(const std::bad_alloc &) {
                return make_tuple(error::out_of_memory, NOT_A_RESPONSE, 0ull);
            }
        }

        
        inline tuple<error_code, ItemPtr> Cache::create_item(const bytes key, const hash_type hash, uint32 value_length, opaque_flags_type flags, expiration_time_point expiration, const version_type ver) noexcept {
            void * memory;
            const size_t size_required = Item::CalcSizeRequired(key, value_length);
            static const auto on_delete = [=](void * ptr) -> void {
                auto i = reinterpret_cast<Item *>(ptr);
                debug_only(bool deleted = ) this->m_dict.del(i->key(), i->hash());
                debug_assert(deleted);
            };
            memory = m_allocator.alloc_or_evict(size_required, settings.cache.has_evictions, on_delete);
            if (memory != nullptr) {
                auto item = new (memory) Item(key, hash, value_length, flags, expiration, ver);
                return make_tuple(error::success, item);
            } else {
                return make_tuple(error::out_of_memory, nullptr);
            }
        }


        inline tuple<error_code, ItemPtr> Cache::create_item_version(ItemPtr existing_item, uint32 new_value_length) noexcept {
            debug_assert(existing_item);
            error_code error; ItemPtr new_item; auto i = existing_item;
            tie(error, new_item) = create_item(i->key(), i->hash(), new_value_length, i->opaque_flags(), i->expiration_time(), i->version() + 1);
            return make_tuple(error, new_item);
        }


        inline void Cache::item_free(ItemPtr item) noexcept {
            m_allocator.free(item);
        }


        inline void Cache::replace_item_at(const iterator at, ItemPtr new_item) noexcept {
            auto old_item = at.value();
            debug_assert(old_item->hash() == new_item->hash() && old_item->key() == new_item->key());
            new_item->new_version_of(old_item);
            item_free(old_item);
            m_dict.remove(at); // remove will not touch freed object
            m_dict.insert(at, new_item->key(), new_item->hash(), new_item);
        }



    } // namespace cache

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_H_INCLUDED

#ifndef CACHELOT_CACHE_H_INCLUDED
#define CACHELOT_CACHE_H_INCLUDED

#include <cachelot/memalloc.h>
#include <cachelot/item.h>
#include <cachelot/dict.h>

/// @defgroup cache Cache implementation
/// @{

namespace cachelot {


    namespace cache {

        /// Single entry in the main cache dict
        class ItemDictEntry {
        public:
            constexpr ItemDictEntry() = default;
            explicit constexpr ItemDictEntry(const Key &, const T & the_item) noexcept : m_item(the_item) {}
            constexpr ItemDictEntry(this_type &&) noexcept = default;
            ItemDictEntry & operator=(this_type &&) noexcept = default;

            // disallow copying
            ItemDictEntry(const this_type &) = delete;
            ItemDictEntry & operator=(const this_type &) = delete;

            // key getter
            const Key & key() const noexcept {
                debug_assert(m_item);
                return m_item->key();
            }

            // value getter
            const T & value() const noexcept {
                debug_assert(m_item);
                return m_item->value();
            }

            // swap with other entry
            void swap(this_type & other) {
                using std::swap;
                swap(m_item, other.m_item);
            }
        private:
            ItemPtr m_item;
        };

        template <typename Key, typename T>
        void swap(ItemDictEntry & left, ItemDictEntry & right) {
            left.swap(right);
        }

        /// Options of the underlying hash table
        struct DictOptions {
            typedef uint32 size_type;
            typedef uint32 hash_type;
            static constexpr size_type max_load_factor_percent = 93;
        };


        /**
         * Cache manages all Items, gives acces by key, maintains item expiration
         */
        class Cache {
           // Underlying dictionary
            typedef dict<bytes, ItemPtr, bytes::equal_to, ItemDictEntry, DictOptions> dict_type;
            typedef dict_type::iterator iterator;
            // Clock types
            typedef chrono::steady_clock clock_type;
            typedef clock_type::time_point expiration_time_type;
        public:
            typedef dict_type::hash_type hash_type;
            typedef chrono::seconds seconds;
            typedef uint16 flags_type;
            typedef uint64 cas_type;

        public:
            /**
             * constructor
             *
             * @param memory_size - amount of memory available for storage use
             * @param initial_dict_size - number of reserved items in dictionary
             */
            Cache(size_t memory_size, size_t initial_dict_size);

            /// destructor
            ~Cache();

            /**
             * Request to retrieve item
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_get(error_code error, bool found, bytes value, flags_type flags, cas_type cas_value)
             * @endcode
             */
            template <typename Callback>
            void async_get(const bytes key, const hash_type hash, Callback on_get) noexcept;


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
             * Request to unconditionally store item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_set(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_set) noexcept;

            /**
             * Request to store non-existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_add(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_add) noexcept;

            /**
             * Request to modify existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_replace(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_replace) noexcept;

            /**
             * Request to compare-and-swap items
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_cas(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_cas) noexcept;

            /**
             * Request to store non-existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_append(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_append) noexcept;

            /**
             * Request to store non-existing item
             *
             * @copydoc doxygen_store_command
             */
            template <typename Callback>
            void async_prepend(const bytes key, const hash_type hash, bytes value, flags_type flags, seconds expires_after, cas_type cas_value, Callback on_prepend) noexcept;

            /**
             * Request to delete item
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_del(error_code error, bool deleted)
             * @endcode
             */
            template <typename Callback>
            void async_del(const bytes key, const hash_type hash, Callback on_del) noexcept;

            /**
             * Request to prolong item lifetime
             *
             * @tparam Callback - callback will be called when request is completed
             * Callback must have following signature:
             * @code
             *    void on_touch(error_code error, bool touched)
             * @endcode
             */
            template <typename Callback>
            void async_touch(const bytes key, const hash_type hash, seconds expires_after, Callback on_touch) noexcept;

        private:
            /**
             * retrieve item from storage taking in account its expiration time,
             * expired items will be immediately removed and retrieve_item() will report that item has not found
             */
            tuple<bool, dict_type::iterator> retrieve_item(const bytes key, const hash_type hash) noexcept;

            /// process `get` request
            void do_get(const GetRequest & req) noexcept;

            /// process `set` request
            void do_set(const StoreRequest & req) noexcept;

            /// process `add` request
            void do_add(const StoreRequest & req) noexcept;

            /// process `replace` request
            void do_replace(const StoreRequest & req) noexcept;

            /// process `cas` request
            void do_cas(const StoreRequest & req) noexcept;

            /// process `append` request
            void do_append(const StoreRequest & req) noexcept;

            /// process `prepend` request
            void do_prepend(const StoreRequest & req) noexcept;

            /// process `del` request
            void do_del(const DelRequest & req) noexcept;

            /// process `touch` request
            void do_touch(const TouchRequest & req) noexcept;

            /// process next request from the queue
            bool process_request() noexcept;

        private:
            std::unique_ptr<byte[]> memory_arena;
            memalloc m_allocator;
            dict_type m_dict;
            mpsc_queue<AsyncRequest> m_requests;
        };


    } // namespace cache

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_H_INCLUDED

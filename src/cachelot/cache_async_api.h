#ifndef CACHELOT_CACHE_ASYNC_API_H_INCLUDED
#define CACHELOT_CACHE_ASYNC_API_H_INCLUDED

#ifndef CACHELOT_CACHE_H_INCLUDED
#  include <cachelot/cache.h>
#endif
#ifndef CACHELOT_MPSC_QUEUE_H_INCLUDED
#  include <cachelot/mpsc_queue.h>
#endif
#include <thread>       // worker thread
#include <functional>   // std::function for callbacks

/// @ingroup cache
/// @{

namespace cachelot { 
    
    namespace cache { 
        /**
         * Thread safe asynchronous cache API
         * @see Cache
         */
        class AsyncCacheAPI {
            // Underlying dictionary
            typedef dict<bytes, ItemPtr, bytes::equal_to, ItemDictEntry, DictOptions> dict_type;
            typedef dict_type::iterator iterator;
        public:
            typedef dict_type::hash_type hash_type;
            typedef dict_type::size_type size_type;
        public:
            /// @copydoc Cache::Cache()
            explicit AsyncCacheAPI(size_t memory_size, size_t initial_dict_size);

            /// stop processing requests
            void terminate();
            
            /// @copydoc Cache::do_get()
            template <typename Callback>
            void do_get(const bytes key, const hash_type hash, Callback on_get) noexcept;
            
            /// @copydoc Cache::do_set()
            template <typename Callback>
            void do_set(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_set) noexcept;
            
            /// @copydoc Cache::do_add()
            template <typename Callback>
            void do_add(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_add) noexcept;
            
            /// @copydoc Cache::do_replace()
            template <typename Callback>
            void do_replace(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_replace) noexcept;
            
            /// @copydoc Cache::do_cas()
            template <typename Callback>
            void do_cas(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_cas) noexcept;
            
            /// @copydoc Cache::do_append()
            template <typename Callback>
            void do_append(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_append) noexcept;
            
            /// @copydoc Cache::do_prepend()
            template <typename Callback>
            void do_prepend(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_prepend) noexcept;
            
            /// @copydoc Cache::do_del()
            template <typename Callback>
            void do_del(const bytes key, const hash_type hash, Callback on_del) noexcept;
            
            /// @copydoc Cache::do_touch()
            template <typename Callback>
            void do_touch(const bytes key, const hash_type hash, seconds expires, Callback on_touch) noexcept;
        
        private:
            void process_requests();
        
        private:
            /// Request arguments holder
            struct RequestArgs;         /// < base class
            struct GetRequestArgs;      /// < `get`
            struct StoreRequestArgs;    /// < `set` / `replace` / `add` / `prepend` / `append`
            struct DelRequestArgs;      /// < `del`
            struct TouchRequestArgs;    /// < `touch`
            struct CasRequestArgs;      /// < `cas`
            struct IncDecRequestArgs;   /// < `inc` / `dec`
            struct InternalRequestArgs; /// < `stat` / `flush`
            
            /// Request types
            enum RequestType {
                REQ_GET,
                REQ_SET,
                REQ_ADD,
                REQ_REPLACE,
                REQ_APPEND,
                REQ_PREPEND,
                REQ_DEL,
                REQ_CAS,
                REQ_INC,
                REQ_DEC,
                REQ_STAT,
                REQ_FLUSH,
                NUM_REQ
            };
            
            /// Asynchronous request to store in requests queue
            class AsyncRequest;
        
        private:
            Cache m_cache;
            mpsc_queue<AsyncRequest> m_requests;
            std::thread m_worker;
            bool m_termination_requested;
        };

} } // namespace cachelot::cache

/// @}

#endif // CACHELOT_CACHE_ASYNC_API_H_INCLUDED

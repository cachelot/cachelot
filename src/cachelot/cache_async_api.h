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

            /// destructor
            /// stop down background thread and wait for its completion
            ~AsyncCacheAPI();

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
            void background_process_requests();

        private:
            /// Request arguments holder
            struct RequestArgs;         /// < base class
            struct GetRequestArgs;      /// < `get`
            struct StoreRequestArgs;    /// < `set` / `replace` / `add` / `prepend` / `append` / `cas`
            struct DelRequestArgs;      /// < `del`
            struct TouchRequestArgs;    /// < `touch`
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
                REQ_CAS,
                REQ_DEL,
                REQ_TOUCH,
                REQ_INC,
                REQ_DEC,
                REQ_STAT,
                REQ_FLUSH,
            };

            /// Asynchronous request to store in requests queue
            struct AsyncRequest;

            /// allocate new AsyncRequest, may throw std::bad_alloc
            AsyncRequest * AllocateAsyncRequest(const RequestType rtype);

            /// free AsyncRequest memory
            void FreeAsyncRequest(AsyncRequest * req);

        private:
            /// create one of store requests
            template <typename Callback>
            void do_store(const RequestType rtype, const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_store) noexcept;


        private:
            Cache m_cache;
            mpsc_queue<AsyncRequest> m_requests;
            //mpsc_queue<AsyncRequest> m_pool;
            volatile bool m_terminated;
            std::thread m_worker;
        };

        struct AsyncCacheAPI::RequestArgs {
            /// requested item key
            const bytes key;
            /// requested item hash
            const hash_type hash;

            /// constructor
            explicit RequestArgs(const bytes the_key, const hash_type the_hash) noexcept
                : key(the_key)
                , hash(the_hash) {}

            // disallow copying
            RequestArgs(const RequestArgs &) = delete;
            RequestArgs & operator=(const RequestArgs &) = delete;
        };

        struct AsyncCacheAPI::GetRequestArgs : public RequestArgs {
            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*found*/, bytes /*value*/, opaque_flags_type /*flags*/, cas_value_type /*cas_value*/)> callback;

            /// constructor
            template <typename Callback>
            explicit GetRequestArgs(const bytes the_key, const hash_type the_hash, Callback the_callback) noexcept
                : RequestArgs(the_key, the_hash)
                , callback(the_callback) {
            }
        };

        struct AsyncCacheAPI::StoreRequestArgs : public RequestArgs {
            /// value of a stored item
            bytes value;

            /// opaque user defined bit flags
            opaque_flags_type flags;

            /// expiration time point
            seconds expires;

            /// unique value for the `CAS` operation
            cas_value_type cas_value;

            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*stored*/)> callback;

            /// constructor
            template <typename Callback>
            explicit StoreRequestArgs(const bytes the_key, const hash_type the_hash,
                                      bytes the_value, opaque_flags_type the_flags, seconds expires_after,
                                      cas_value_type the_cas_value, Callback the_callback) noexcept
                : RequestArgs(the_key, the_hash)
                , value(the_value)
                , flags(the_flags)
                , expires(expires_after)
                , cas_value(the_cas_value)
                , callback(the_callback) {
            }

        };


        struct AsyncCacheAPI::DelRequestArgs : public RequestArgs {
            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*deleted*/)> callback;

            /// constructor
            template <typename Callback>
            constexpr explicit DelRequestArgs(const bytes the_key, const hash_type the_hash, Callback the_callback) noexcept
                : RequestArgs(the_key, the_hash)
                , callback(the_callback) {
            }
        };


        struct AsyncCacheAPI::TouchRequestArgs : public RequestArgs {
            /// expiration time point
            seconds expires;

            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*touched*/)> callback;

            /// constructor
            template <typename Callback>
            explicit TouchRequestArgs(const bytes the_key, const hash_type the_hash, seconds expires_after, Callback the_callback) noexcept
                : RequestArgs(the_key, the_hash)
                , expires(expires_after)
                , callback(the_callback) {
            }
        };


        struct AsyncCacheAPI::AsyncRequest : public mpsc_queue<AsyncCacheAPI::AsyncRequest>::node {
            /// request arguments
            union {
                GetRequestArgs get_args;
                StoreRequestArgs store_args;
                DelRequestArgs del_args;
                TouchRequestArgs touch_args;
            };

            /// type of request
            const RequestType type;
            
            /// constructor
            explicit AsyncRequest(const RequestType the_type) noexcept
                : type(the_type) {
            }
            
            /// destructor
            ~AsyncRequest() {}
        };

        template <typename Callback>
        inline void AsyncCacheAPI::do_get(const bytes key, const hash_type hash, Callback on_get) noexcept {
            debug_assert(not m_terminated);
            try {
                AsyncRequest * req = AllocateAsyncRequest(REQ_GET);
                new (&req->get_args) GetRequestArgs(key, hash, on_get);
                m_requests.enqueue(req);
            } catch (const std::bad_alloc &) {
                on_get(error::out_of_memory, false, bytes(), opaque_flags_type(), cas_value_type());
            }
        }

        template <typename Callback>
        inline void AsyncCacheAPI::do_store(const RequestType rtype, const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_store) noexcept {
            debug_assert(not m_terminated);
            try {
                AsyncRequest * req = AllocateAsyncRequest(rtype);
                new (&req->store_args) StoreRequestArgs(key, hash, value, flags, expires, cas_value, on_store);
                m_requests.enqueue(req);
            } catch (const std::bad_alloc &) {
                on_store(error::out_of_memory, false);
            }
        }

        template <typename Callback>
        inline void AsyncCacheAPI::do_set(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_set) noexcept {
            do_store(REQ_SET, key, hash, value, flags, expires, cas_value, on_set);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_add(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_add) noexcept {
            do_store(REQ_ADD, key, hash, value, flags, expires, cas_value, on_add);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_replace(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_replace) noexcept {
            do_store(REQ_REPLACE, key, hash, value, flags, expires, cas_value, on_replace);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_cas(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_cas) noexcept {
            do_store(REQ_CAS, key, hash, value, flags, expires, cas_value, on_cas);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_append(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_append) noexcept {
            do_store(REQ_APPEND, key, hash, value, flags, expires, cas_value, on_append);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_prepend(const bytes key, const hash_type hash, bytes value, opaque_flags_type flags, seconds expires, cas_value_type cas_value, Callback on_prepend) noexcept {
            do_store(REQ_PREPEND, key, hash, value, flags, expires, cas_value, on_prepend);
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_del(const bytes key, const hash_type hash, Callback on_del) noexcept {
            debug_assert(not m_terminated);
            try {
                AsyncRequest * req = AllocateAsyncRequest(REQ_DEL);
                new (&req->del_args) DelRequestArgs(key, hash, on_del);
                m_requests.enqueue(req);
            } catch (const std::bad_alloc &) {
                on_del(error::out_of_memory, false);
            }
        }


        template <typename Callback>
        inline void AsyncCacheAPI::do_touch(const bytes key, const hash_type hash, seconds expires, Callback on_touch) noexcept {
            debug_assert(not m_terminated);
            try {
                AsyncRequest * req = AllocateAsyncRequest(REQ_TOUCH);
                new (&req->touch_args) TouchRequestArgs(key, hash, expires, on_touch);
                m_requests.enqueue(req);
            } catch (const std::bad_alloc &) {
                on_touch(error::out_of_memory, false);
            }
        }


} } // namespace cachelot::cache

/// @}

#endif // CACHELOT_CACHE_ASYNC_API_H_INCLUDED

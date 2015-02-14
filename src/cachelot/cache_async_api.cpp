#include <cachelot/cachelot.h>
#include <cachelot/cache_async_api.h>

namespace cachelot { namespace cache {

    AsyncCacheAPI::AsyncCacheAPI(size_t memory_size, size_t initial_dict_size)
        : m_cache(memory_size, initial_dict_size)
        , m_requests()
        //, m_pool()
        , m_terminated(false)
        , m_worker(&AsyncCacheAPI::background_process_requests, this)
    {}


    AsyncCacheAPI::~AsyncCacheAPI() {
        // interrupt worker thread
        debug_assert(not m_terminated);
        m_terminated = true;
        try { m_worker.join(); } catch (...) { /* DO NOTHING */ }
        // delete unprocessed requests
        AsyncRequest * req = m_requests.dequeue();
        while (req != nullptr) {
            FreeAsyncRequest(req);
            req = m_requests.dequeue();
        }
    }

    AsyncCacheAPI::AsyncRequest * AsyncCacheAPI::AllocateAsyncRequest(const RequestType rtype) {
        void * memory = (AsyncRequest *)std::malloc(sizeof(AsyncRequest));
        if (memory != nullptr) {
            return new (memory) AsyncRequest(rtype);
        } else {
            throw std::bad_alloc();
        }
    }


    void AsyncCacheAPI::FreeAsyncRequest(AsyncCacheAPI::AsyncRequest * req) {
        debug_assert(req != nullptr);
        std::free(req);
    }

    void AsyncCacheAPI::background_process_requests() {
        do {
            AsyncRequest * req = m_requests.dequeue();
            if (req != nullptr) {
                switch (req->type) {
                case REQ_GET: {
                    auto & args = req->get_args;
                    m_cache.do_get(args.key, args.hash, args.callback);
                    args.~GetRequestArgs();
                    break;
                }
                case REQ_SET: {
                    auto & args = req->store_args;
                    m_cache.do_set(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_ADD: {
                    auto & args = req->store_args;
                    m_cache.do_add(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_REPLACE: {
                    auto & args = req->store_args;
                    m_cache.do_replace(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_APPEND: {
                    auto & args = req->store_args;
                    m_cache.do_append(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_PREPEND: {
                    auto & args = req->store_args;
                    m_cache.do_prepend(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_CAS: {
                    auto & args = req->store_args;
                    m_cache.do_cas(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    args.~StoreRequestArgs();
                    break;
                }
                case REQ_DEL: {
                    auto & args = req->del_args;
                    m_cache.do_del(args.key, args.hash, args.callback);
                    args.~DelRequestArgs();
                    break;
                }
                case REQ_TOUCH: {
                    auto & args = req->touch_args;
                    m_cache.do_touch(args.key, args.hash, args.expires, args.callback);
                    args.~TouchRequestArgs();
                    break;
                }
                case REQ_INC:
                    break;
                case REQ_DEC:
                    break;
                case REQ_STAT:
                    break;
                case REQ_FLUSH:
                    break;
                }
                FreeAsyncRequest(req);
            } else {
                /* TODO: SLEEP */
            }
        } while (not m_terminated);
    }


} } // namespace cachelot::cache


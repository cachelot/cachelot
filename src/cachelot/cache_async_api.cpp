#include <cachelot/cachelot.h>
#include <cachelot/cache_async_api.h>

#include <emmintrin.h>  // _mm_pause

namespace cachelot { namespace cache {

    AsyncCacheAPI::AsyncCacheAPI(size_t memory_size, size_t initial_dict_size)
        : m_cache(memory_size, initial_dict_size)
        , m_requests()
        , m_terminated(false)
        , m_waiting(false)
        // must be initialised last
        , m_worker(&AsyncCacheAPI::background_process_requests, this)
    {}


    AsyncCacheAPI::~AsyncCacheAPI() {
        // interrupt worker thread
        debug_assert(not m_terminated);
        m_terminated = true;
        m_waiting_cndvar.notify_one();
        try { m_worker.join(); } catch (...) { /* DO NOTHING */ }
        // delete unprocessed requests
        AsyncRequest * req = m_requests.dequeue();
        while (req != nullptr) {
            delete req;
            req = m_requests.dequeue();
        }
    }


    void AsyncCacheAPI::background_process_requests() {
        constexpr static uint64 max_spin = 2000;
        uint64 spin_count = 0;
        do {
            std::unique_ptr<AsyncRequest> req(m_requests.dequeue());
            if (req != nullptr) {
                switch (req->type) {
                case REQ_GET: {
                    auto & args = req->get_args;
                    m_cache.do_get(args.key, args.hash, args.callback);
                    break;
                }
                case REQ_SET: {
                    auto & args = req->store_args;
                    m_cache.do_set(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_ADD: {
                    auto & args = req->store_args;
                    m_cache.do_add(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_REPLACE: {
                    auto & args = req->store_args;
                    m_cache.do_replace(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_APPEND: {
                    auto & args = req->store_args;
                    m_cache.do_append(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_PREPEND: {
                    auto & args = req->store_args;
                    m_cache.do_prepend(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_CAS: {
                    auto & args = req->store_args;
                    m_cache.do_cas(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                    break;
                }
                case REQ_DEL: {
                    auto & args = req->del_args;
                    m_cache.do_del(args.key, args.hash, args.callback);
                    break;
                }
                case REQ_TOUCH: {
                    auto & args = req->touch_args;
                    m_cache.do_touch(args.key, args.hash, args.expires, args.callback);
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
                default:
                    break;
                }
            } else {
                _mm_pause();
                spin_count += 1;
                if (spin_count < max_spin) {
                    continue;
                }
                // we've waited long enough
                m_waiting.store(true, std::memory_order_relaxed);
                std::unique_lock<std::mutex> lck(m_wait_mutex);
                m_waiting_cndvar.wait(lck);
                spin_count = 0;
                m_waiting.store(false, std::memory_order_relaxed);
            }
        } while (not m_terminated);
    }


    void AsyncCacheAPI::enqueue(AsyncRequest * req) noexcept {
        m_requests.enqueue(req);
        if (m_waiting.load(std::memory_order_relaxed)) {
            m_waiting_cndvar.notify_one();
        }
    }


} } // namespace cachelot::cache


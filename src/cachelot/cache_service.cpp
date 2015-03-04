#include <cachelot/cachelot.h>
#include <cachelot/cache_service.h>
#include <cachelot/io_serialization.h>

#include <emmintrin.h>  // _mm_pause

namespace cachelot { namespace cache {


    constexpr bytes CRLF = bytes::from_literal("\r\n");

    CacheService::CacheService(size_t memory_size, size_t initial_dict_size)
        : Actor(&CacheService::main)
        , m_cache(memory_size, initial_dict_size) {
    }


    CacheService::~CacheService() {}


    bool CacheService::main() noexcept {
        auto req = receive_message();
        if (req != nullptr) {
            switch (req->type) {
            case atom("get"): {
                bytes key; hash_type hash; atom_type serialization; io_buffer * iobuf;
                req->unpack(key, hash, serialization, iobuf);
                m_cache.do_get(key, hash, [&req, this, key, serialization, iobuf](error_code error, bool found, bytes value, opaque_flags_type flags, cas_value_type cas_value) -> void {
                    if (not error) {
                        if (found) {
                            switch (serialization) {
                            case atom("text"): {
                                io_stream<text_serialization_tag> out(*iobuf);
                                const auto value_length = static_cast<unsigned>(value.length());
                                static const bytes VALUE = bytes::from_literal("VALUE ");
                                out << VALUE << key << ' ' << flags << ' ' << value_length;
                                if (cas_value != cache::cas_value_type()) {
                                    out << ' ' << cas_value;
                                }
                                out << CRLF << value << CRLF;
                                this->reply_to(std::move(req), atom("item"));
                                break;
                            }
                            default:
                                debug_assert(false && "Invalid serialization");
                            }
                        } else {
                            this->reply_to(std::move(req), atom("notfound"));
                        }
                    } else {
                        this->reply_to(std::move(req), atom("error"), error);
                    }
                });
                break;
            }
            case atom("set"): {
                bytes key; hash_type hash; bytes value; opaque_flags_type flags; seconds expires; cas_value_type cas_value; bool noreply;
                req->unpack(key, hash, value, flags, expires, cas_value, noreply);
                m_cache.do_set(key, hash, value, flags, expires, cas_value, [&req, this, noreply](error_code error, bool stored) {
                    if (not error) {
                        if (not noreply) {
                            this->reply_to(std::move(req), stored ? atom("stored") : atom("not_stored"));
                        }
                    } else {
                        this->reply_to(std::move(req), atom("error"), error);
                    }
                });
                break;
            }
            case atom("add"): {
                //auto & args = req->store_args;
                //m_cache.do_add(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                break;
            }
            case atom("replace"): {
                //auto & args = req->store_args;
                //m_cache.do_replace(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                break;
            }
            case atom("append"): {
                //auto & args = req->store_args;
                //m_cache.do_append(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                break;
            }
            case atom("prepend"): {
//                    auto & args = req->store_args;
//                    m_cache.do_prepend(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                break;
            }
            case atom("cas"): {
//                    auto & args = req->store_args;
//                    m_cache.do_cas(args.key, args.hash, args.value, args.flags, args.expires, args.cas_value, args.callback);
                break;
            }
            case atom("delete"): {
//                    auto & args = req->del_args;
//                    m_cache.do_del(args.key, args.hash, args.callback);
                break;
            }
            case atom("touch"): {
//                    auto & args = req->touch_args;
//                    m_cache.do_touch(args.key, args.hash, args.expires, args.callback);
                break;
            }
            case atom("sync"): {
                atom_type return_marker;
                req->unpack(return_marker);
                this->reply_to(std::move(req), return_marker);
                break;
            }
            default:
                debug_assert(false && "Unknown request");
                break;
            }
            return true;
        } else {
            return false;
        }
    }


} } // namespace cachelot::cache


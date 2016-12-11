//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#include <cachelot/common.h>
#include <cachelot/c_api.h>
#include <cachelot/cache.h>
#include <cachelot/version.h>


using namespace cachelot;

extern "C" {

    const uint32_t cachelot_infinite_TTL = cachelot::cache::Item::infinite_TTL.count();
    static const int unknown_error_code = static_cast<int>(error::unknown_error);
    static const char * unknown_error_category = "Unknown";


    struct cachelot_t {
        cache::Cache cache;
    };



    struct cachelot_item_t {
        unsigned char __filler[sizeof(cache::Item)];
    };


    inline void none_error(CachelotError * out_err) noexcept {
        if (out_err == nullptr) {
            return;
        }
        out_err->code = 0;
        out_err->category = nullptr;
        out_err->desription[0] = '\0';
    }

    inline void system_error_to_err_struct(const system_error & err, CachelotError * out_err) noexcept {
        if (out_err == nullptr) {
            return;
        }
        auto code = err.code();
        const auto & category = code.category();
        out_err->code = code.value();
        out_err->category = category.name();
        auto desc = category.message(code.value());
        strncpy((char *)out_err->desription, desc.c_str(), sizeof(CachelotError::desription));
    }

    inline void std_exception_to_err_struct(const std::exception & err, CachelotError * out_err) noexcept {
        if (out_err == nullptr) {
            return;
        }
        out_err->code = unknown_error_code;
        out_err->category = unknown_error_category;
        strncpy((char *)out_err->desription, err.what(), sizeof(CachelotError::desription));
    }

    inline void unknown_exception_to_err_struct(CachelotError * out_err) noexcept {
        if (out_err == nullptr) {
            return;
        }
        out_err->code = unknown_error_code;
        out_err->category = unknown_error_category;
        static const char msg[] = "unknown";
        strncpy((char *)out_err->desription, msg, sizeof(CachelotError::desription));
    }


    CachelotPtr cachelot_init(CachelotOptions opts, CachelotError * out_error) {
        try {
            cachelot_t * c = new cachelot_t{cache::Cache::Create(opts.memory_limit, opts.mem_page_size, opts.initial_dict_size, opts.enable_evictions)};
            return c;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return nullptr;
    }


    void cachelot_destroy(CachelotPtr c) {
        if (c != nullptr) {
            delete c;
        }
    }


    CachelotItemPtr cachelot_create_item_raw(CachelotPtr c, CachelotItemKey k, const char * value, size_t valuelen, CachelotError * out_error) {
        try {
            auto new_item = c->cache.create_item(slice(k.key, k.keylen), k.hash, valuelen, 0, cache::Item::infinite_TTL);
            new_item->assign_value(slice(value, valuelen));
            return reinterpret_cast<CachelotItemPtr>(new_item);
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return nullptr;
    }


    void cachelot_destroy_item(CachelotPtr c, CachelotItemPtr i) {
        if (i != nullptr) {
            c->cache.destroy_item(reinterpret_cast<cache::ItemPtr>(i));
        }
    }


    uint32_t cachelot_item_get_ttl_seconds(CachelotConstItemPtr i) {
        auto item = reinterpret_cast<cache::ConstItemPtr>(i);
        return static_cast<uint32_t>(item->ttl().count());
    }


    void cachelot_item_set_ttl_seconds(CachelotItemPtr i, uint32_t keepalive_sec) {
        auto item = reinterpret_cast<cache::ItemPtr>(i);
        item->set_ttl(cachelot::cache::seconds(keepalive_sec));
    }


    const char * cachelot_item_get_key(const CachelotConstItemPtr i) {
        auto item = reinterpret_cast<cache::ConstItemPtr>(i);
        return item->key().begin();
    }


    size_t cachelot_item_get_keylen(const CachelotConstItemPtr i) {
        auto item = reinterpret_cast<cache::ConstItemPtr>(i);
        return item->key().length();
    }


    const char * cachelot_item_get_value(const CachelotConstItemPtr i) {
        auto item = reinterpret_cast<cache::ConstItemPtr>(i);
        return item->value().begin();
    }


    size_t cachelot_item_get_valuelen(const CachelotConstItemPtr i) {
        auto item = reinterpret_cast<cache::ConstItemPtr>(i);
        return item->value().length();
    }


    bool cachelot_set(CachelotPtr c, CachelotItemPtr i, CachelotError * out_error) {
        try {
            auto item = reinterpret_cast<cache::ItemPtr>(i);
            c->cache.do_set(item);
            none_error(out_error);
            return true;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    CachelotConstItemPtr cachelot_get_unsafe(CachelotPtr c, CachelotItemKey k, CachelotError * out_error) {
        try {
            cache::ConstItemPtr item = c->cache.do_get(slice(k.key, k.keylen), k.hash);
            none_error(out_error);
            return reinterpret_cast<CachelotConstItemPtr>(item);
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return nullptr;
    }


    bool cachelot_add(CachelotPtr c, CachelotItemPtr i, CachelotError * out_error) {
        try {
            auto item = reinterpret_cast<cache::ItemPtr>(i);
            auto ret = c->cache.do_add(item);
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_replace(CachelotPtr c, CachelotItemPtr i, CachelotError * out_error) {
        try {
            auto item = reinterpret_cast<cache::ItemPtr>(i);
            auto ret = c->cache.do_replace(item);
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_append(CachelotPtr c, CachelotItemPtr i, CachelotError * out_error) {
        try {
            auto item = reinterpret_cast<cache::ItemPtr>(i);
            auto ret = c->cache.do_append(item);
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_prepend(CachelotPtr c, CachelotItemPtr i, CachelotError * out_error) {
        try {
            auto item = reinterpret_cast<cache::ItemPtr>(i);
            auto ret = c->cache.do_prepend(item);
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_delete(CachelotPtr c, CachelotItemKey k, CachelotError * out_error) {
        try {
            auto ret = c->cache.do_delete(slice(k.key, k.keylen), k.hash);
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_touch(CachelotPtr c, CachelotItemKey k, uint32_t keepalive_sec, CachelotError * out_error) {
        try {
            auto ret = c->cache.do_touch(slice(k.key, k.keylen), k.hash, cache::seconds(keepalive_sec));
            none_error(out_error);
            return ret;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_incr(CachelotPtr c, CachelotItemKey k, uint64_t delta, uint64_t * result, CachelotError * out_error) {
        try {
            bool found; uint64 newval;
            tie(found, newval) = c->cache.do_incr(slice(k.key, k.keylen), k.hash, delta);
            if (result != nullptr) {
                *result = newval;
            }
            return found;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    bool cachelot_decr(CachelotPtr c, CachelotItemKey k, uint64_t delta, uint64_t * result, CachelotError * out_error) {
        try {
            bool found; uint64 newval;
            tie(found, newval) = c->cache.do_decr(slice(k.key, k.keylen), k.hash, delta);
            if (result != nullptr) {
                *result = newval;
            }
            return found;
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
        return false;
    }


    void cachelot_flush_all(CachelotPtr c, CachelotError * out_error) {
        try {
            c->cache.do_flush_all();
        } catch (const system_error & e) {
            system_error_to_err_struct(e, out_error);
        } catch(const std::exception & e) {
            std_exception_to_err_struct(e, out_error);
        } catch (...) {
            unknown_exception_to_err_struct(out_error);
        }
    }

    uint32_t cachelot_hash(const char * key, size_t keylen) {
        cache::HashFunction calc_hash;
        return calc_hash(slice(key, keylen));
    }

    const char * cachelot_version() {
        return CACHELOT_VERSION_FULL;
    }


} // extern "C"


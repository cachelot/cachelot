#ifndef CACHELOT_C_API_H_INCLUDED
#define CACHELOT_C_API_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/**
 * @defgroup c_api Cachelot C language API
 *
 * @warning This API is *not* thread safe
 *
 *
 * Please keep in mind that Cachelot uses its own memory space. All stored items are in this separate space.
 * Sometimes Cachelot may return pointer to an item in its memory to avoid unnecessary copying.
 *
 * @warning Such pointers are guarenteed to be valid only until the next call to Cachelot.
 *
 */
/** @{ */

struct cachelot_t;
struct cachelot_item_t;


/** Pointer to the cache */
typedef struct cachelot_t * CachelotPtr;

/** Pointer to the cache Item */
typedef struct cachelot_item_t * CachelotItemPtr;
/** Pointer to the read-only cache Item */
typedef const struct cachelot_item_t * CachelotConstItemPtr;

/** Special value for Item TTL - never expire */
extern const uint32_t cachelot_infinite_TTL;

/** Item eviction callback type */
typedef void (*CachelotOnEvictedCallback)(CachelotConstItemPtr);


/** Cache created with the options below */
typedef struct cachelot_options_t {
    /** amount of memory to pre-allocate and use (power of 2) */
    size_t memory_limit;

    /** lesser page_size leads to more accurate eviction but also limits the max item size (power of 2) */
    size_t mem_page_size;

    /** initial size of the hash table (power of 2) */
    size_t initial_dict_size;

    /** whether to evict item when out of memory or just report an error */
    bool enable_evictions;
} CachelotOptions;


/** Some calls may end up with an error */
typedef struct cachelot_error_t {
    /** Error code (unique only across one category) */
    int code;
    /** Error category name */
    const char * category;
    /** Detailed error description */
    char desription[255];
} CachelotError;

/** Key to access cache items */
typedef struct cachelot_item_key_t {
    const char * key;
    size_t keylen;
    uint32_t hash;
} CachelotItemKey;


/** Create new cache */
CachelotPtr cachelot_init(CachelotOptions opts, CachelotError * out_error);

/** Destroy previously created cache */
void cachelot_destroy(CachelotPtr c);

/**
 * Create new Item in the Cachelot memory region, allows to pass pre-calculated hash
 *
 * @warning returned pointer guaranteed to be valid only *until* the next Cachelot call
 */
CachelotItemPtr cachelot_create_item_raw(CachelotPtr c, CachelotItemKey key, const char * value, size_t valuelen, CachelotError * error);

/**
 * Destroy Item created with `cachelot_create_item_raw`
 *
 * @note Usually it's not necessary to explicitly destroy an Item.
 * Any call with an CachelotItemPtr argument will take ownership of the pointer.
 * Although if Item was created and not passed as an argument
 */
void cachelot_destroy_item(CachelotPtr c, CachelotItemPtr i);

/** Retrieve Item Time-To-Live in seconds */
uint32_t cachelot_item_get_ttl_seconds(const CachelotConstItemPtr i);

/** Assign Item Time-To-Live in seconds */
void cachelot_item_set_ttl_seconds(CachelotItemPtr i, uint32_t keepalive_seconds);

/** Retrieve Item key */
const char * cachelot_item_get_key(const CachelotConstItemPtr i);

/** Retrieve Item key length */
size_t cachelot_item_get_keylen(const CachelotConstItemPtr i);

/** Retrieve Item value */
const char * cachelot_item_get_value(const CachelotConstItemPtr i);

/** Retrieve Item value length */
size_t cachelot_item_get_valuelen(const CachelotConstItemPtr i);

//! @copydoc cachelot::cache::Cache::do_set
bool cachelot_set(CachelotPtr c, CachelotItemPtr i, CachelotError * error);


/**
 * Retrieve pointer to Item from cache (if any)
 * @return
 *     - if key-hash pair was found returns pointer CachelotConstItemPtr.
 *     - returns NULL if none was found
 * @warning pointer is only valid until the next cache call
 */
CachelotConstItemPtr cachelot_get_unsafe(CachelotPtr c, CachelotItemKey key, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_add */
bool cachelot_add(CachelotPtr c, CachelotItemPtr i, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_replace */
bool cachelot_replace(CachelotPtr c, CachelotItemPtr i, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_append */
bool cachelot_append(CachelotPtr c, CachelotItemPtr i, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_prepend */
bool cachelot_prepend(CachelotPtr c, CachelotItemPtr i, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_delete */
bool cachelot_delete(CachelotPtr c, CachelotItemKey key, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_touch */
bool cachelot_touch(CachelotPtr c, CachelotItemKey key, uint32_t keepalive_sec, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_incr */
bool cachelot_incr(CachelotPtr c, CachelotItemKey key, uint64_t delta, uint64_t * result, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_decr */
bool cachelot_decr(CachelotPtr c, CachelotItemKey key, uint64_t delta, uint64_t * result, CachelotError * error);

/** @copydoc cachelot::cache::Cache::do_flush_all */
void cachelot_flush_all(CachelotPtr c, CachelotError * error);

/**
 * assign eviction callback
 * @code
 * void OnEvictedCallback(CachelotConstItemPtr i);
 * @endcode
 *
 * setting `cb` argument to `NULL` will reset previously assigned callback
 */
void cachelot_on_eviction_callback(CachelotPtr c, CachelotOnEvictedCallback cb);

/** default cachelot hash function */
uint32_t cachelot_hash(const char * key, size_t keylen);

/** retrieve Cachelot version */
const char * cachelot_version();

/** @} */

#ifdef __cplusplus
}
#endif

#endif // CACHELOT_C_API_H_INCLUDED

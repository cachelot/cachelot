#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <windows.h>
#define sleep Sleep
#else
#include <unistd.h>
#endif
#include <cachelot/c_api.h>



static const CachelotOptions options = {
    .memory_limit = (size_t)4*1024*1024*1024,
    .mem_page_size = 1024,
    .initial_dict_size = 2048,
    .enable_evictions = true,
};


void print_cachelot_error(const char * msg, const CachelotError * e) {
    printf("%s: error code=%d category='%s': %s\n", msg, e->code, e->category, e->desription);
}


CachelotItemKey new_key(const char * key) {
    size_t keylen = strlen(key);
    uint32_t hash = cachelot_hash(key, keylen);
    CachelotItemKey k = {key, keylen, hash};
    return k;
}

CachelotItemPtr new_item(CachelotPtr c, const char * key, const char * value, CachelotError * out_err) {
    CachelotItemKey k = new_key(key);
    CachelotItemPtr i = cachelot_create_item_raw(c, k, value, strlen(value), out_err);
    if (i == NULL) {
        print_cachelot_error("Failed to create new item", out_err);
    }
    return i;
}

bool check_value_eq(CachelotPtr c, const char * key, const char * value, CachelotError * out_err) {
    CachelotConstItemPtr i;
    CachelotItemKey k = new_key(key);
    i = cachelot_get_unsafe(c, k, out_err);
    if (i == NULL) {
        printf("Failed to retrieve key '%s'", key);
        print_cachelot_error(":", out_err);
        return false;
    }
    if (strncmp(cachelot_item_get_value(i), value, strlen(value)) != 0) {
        printf("cache value '%s' != expected '%s'\n", cachelot_item_get_value(i), value);
        return false;
    }
    return true;
}

#define CHECK_EQ(key, value) if (!check_value_eq(c, key, value, out_err)) { return false; }


#define NEW_ITEM(varname, key, value) do {      \
    varname = new_item(c, key, value, out_err); \
    if (varname == NULL) return false;          \
} while(false)

#define CACHELOT_SET(varname) do {                              \
    if (!cachelot_set(c, varname, out_err) ) {                  \
        print_cachelot_error("cachelot_set failed", out_err);   \
        return false;                                           \
    }                                                           \
} while(false)

bool test_basic(CachelotPtr c, CachelotError * out_err) {
    CachelotItemPtr i1;
    // Add new Item
    NEW_ITEM(i1, "Item1", "Value1");
    if (!cachelot_add(c, i1, out_err) ) {
        print_cachelot_error("Item was not added", out_err);
        return false;
    }
    // Try to add the same key
    NEW_ITEM(i1, "Item1", "Value2");
    if (cachelot_add(c, i1, out_err) ) {
        print_cachelot_error("Item with the same key was added", out_err);
        return false;
    }
    // retrieve Item back
    CHECK_EQ("Item1", "Value1");


    // replace value
    NEW_ITEM(i1, "Item1", "New Value");
    if (!cachelot_replace(c, i1, out_err)) {
        print_cachelot_error("Failed to replace stored item", out_err);
        return false;
    }
    // try to replace non-existing
    NEW_ITEM(i1, "Does not exist", "");
    if (cachelot_replace(c, i1, out_err)) {
        print_cachelot_error("Non-existing item was replaced", out_err);
        return false;
    }
    // retrieve again
    CHECK_EQ("Item1", "New Value");


    // set unconditionaly
    NEW_ITEM(i1, "Item1", "Even newer value");
    if (!cachelot_set(c, i1, out_err) ) {
        print_cachelot_error("Failed to set new item", out_err);
        return false;
    }
    // retrieve again
    CHECK_EQ("Item1", "Even newer value");
    return true;
}


bool test_arith(CachelotPtr c, CachelotError * out_err) {
    CachelotItemPtr i1;
    // create new item with int value
    NEW_ITEM(i1, "IntValue", "00042");
    CACHELOT_SET(i1);

    // happy path
    do {
        CachelotItemKey k = new_key("IntValue");
        uint64_t result;
        // inrement
        if (!cachelot_incr(c, k, 1, &result, out_err)) {
            print_cachelot_error("Failed to increment", out_err);
            return false;
        }
        if (result != 43) {
            print_cachelot_error("Result is not 43", out_err);
            return false;
        }
        CHECK_EQ("IntValue", "43");
        // decrement
        if (!cachelot_decr(c, k, 1, &result, out_err)) {
            print_cachelot_error("Failed to decrement", out_err);
            return false;
        }
        if (result != 42) {
            print_cachelot_error("Result is not 42", out_err);
            return false;
        }
        CHECK_EQ("IntValue", "42");
    } while(false);

    // Invalid integer
    do {
        NEW_ITEM(i1, "Invalid", "_!nt8gr_");
        CACHELOT_SET(i1);
        CachelotItemKey k = new_key("Invalid");
        uint64_t result;
        // increment
        out_err->code = 0; out_err->category = NULL; out_err->desription[0] = '\0';
        if (cachelot_incr(c, k, 1, &result, out_err)) {
            print_cachelot_error("increment invalid value should fail", out_err);
            return false;
        }
        if (out_err->code != 2 ||
            strncmp(out_err->category, "Application error", strlen("Application error")) != 0)
        {
            print_cachelot_error("Unexpected error -->", out_err);
            return false;
        }
        CHECK_EQ("IntValue", "42");
        // decrement
        out_err->code = 0; out_err->category = NULL; out_err->desription[0] = '\0';
        if (cachelot_decr(c, k, 1, &result, out_err)) {
            print_cachelot_error("decrement invalid value should fail", out_err);
            return false;
        }
        if (out_err->code != 2 ||
            strncmp(out_err->category, "Application error", strlen("Application error")) != 0)
        {
            print_cachelot_error("Unexpected error -->", out_err);
            return false;
        }
        CHECK_EQ("IntValue", "42");

    } while(false);

    // Non-existing item
    do {
        CachelotItemKey k = new_key("Non-Existing-Item");
        uint64_t result;
        // increment
        if (cachelot_incr(c, k, 1, &result, out_err)) {
            print_cachelot_error("increment non-existing key should fail", out_err);
            return false;
        }
        // decrement
        if (cachelot_decr(c, k, 1, &result, out_err)) {
            print_cachelot_error("decrement non-existing key should fail", out_err);
            return false;
        }
    } while(false);
    return true;
}


bool test_expiration(CachelotPtr c, CachelotError * out_err) {
    do {
        // Custom TTL
        CachelotItemPtr i1;
        NEW_ITEM(i1, "1sec Item", "Some value. Whatever.");
        cachelot_item_set_ttl_seconds(i1, 1); // TTL 1 sec
        CACHELOT_SET(i1);
        CHECK_EQ("1sec Item", "Some value. Whatever.");
        sleep(1);
        const CachelotItemKey k1 = new_key("1sec Item");
        if (cachelot_get_unsafe(c, k1, out_err) != NULL) {
            print_cachelot_error("Expired item is still in cache", out_err);
            return false;
        }
    } while (false);
    do {
        // touch
        CachelotItemPtr i2;
        NEW_ITEM(i2, "Touch test", "Some value. Whatever.");
        CACHELOT_SET(i2);
        CHECK_EQ("Touch test", "Some value. Whatever.");
        CachelotItemKey k2 = new_key("Touch test");
        if (!cachelot_touch(c, k2, 2, out_err)) {
            print_cachelot_error("cachelot_touch failed", out_err);
            return false;
        }
        sleep(1);
        CHECK_EQ("Touch test", "Some value. Whatever.");
        sleep(1);
        if (cachelot_get_unsafe(c, k2, out_err) != NULL) {
            print_cachelot_error("Expired item is still in cache", out_err);
            return false;
        }
    } while (false);

    return true;
}


bool test_item_functions(CachelotPtr c, CachelotError * out_err) {
    CachelotItemPtr i1;
    const char * key = "The Key!";
    const char * value = "Value for the Great Jusice!";
    NEW_ITEM(i1, key, value);
    bool ret = true;
    // Check key
    if (strncmp(key, cachelot_item_get_key(i1), strlen(key)) != 0 ) {
        printf("cachelot_item_get_key failed: '%s' != '%s'\n", key, cachelot_item_get_key(i1));
        ret = false; goto cleanup;
    }
    if (strlen(key) != cachelot_item_get_keylen(i1)) {
        printf("cachelot_item_get_key failed: '%zu' != '%zu'\n", strlen(key), cachelot_item_get_keylen(i1));
        ret = false; goto cleanup;
    }
    // Check value
    if (strncmp(value, cachelot_item_get_value(i1), strlen(value)) != 0 ) {
        printf("cachelot_item_get_value failed: '%s' != '%s'\n", value, cachelot_item_get_value(i1));
        ret = false; goto cleanup;
    }
    if (strlen(value) != cachelot_item_get_valuelen(i1)) {
        printf("cachelot_item_get_value failed: '%zu' != '%zu'\n", strlen(value), cachelot_item_get_valuelen(i1));
        ret = false; goto cleanup;
    }
    // Check TTL
    if (cachelot_item_get_ttl_seconds(i1) != cachelot_infinite_TTL) {
        printf("cachelot_item_(get/set)_ttl_seconds failed: '%u' != '%u'\n", cachelot_infinite_TTL, cachelot_item_get_ttl_seconds(i1));
        ret = false; goto cleanup;
    }
    cachelot_item_set_ttl_seconds(i1, 42);
    if (cachelot_item_get_ttl_seconds(i1) != 42) {
        printf("cachelot_item_(get/set)_ttl_seconds failed: '%u' != '%u'\n", 42, cachelot_item_get_ttl_seconds(i1));
        ret = false; goto cleanup;
    }
cleanup:
    cachelot_destroy_item(c, i1);
    return ret;
}


static bool __test_cb_on_eviction_callback_succeded = true;
static bool __at_this_point_callback_should_not_be_called = false;
static const char * __large_value = "There must be value large enough to fill 256b page. So there will be one long value that will do it. The other values are just too small, so this one should be long enough.";
void __test_cb_on_eviction_callback(CachelotConstItemPtr i) {
    if (__at_this_point_callback_should_not_be_called) {
        printf("on_eviction_callback was called, while it shouldn't\n");
        __test_cb_on_eviction_callback_succeded = false;
    }
    // key=ItemX
    if (cachelot_item_get_keylen(i) != 6 || strncmp("Item", cachelot_item_get_key(i), 4u) != 0) {
        printf("on_eviction_callback unexpected key: '%s' != '%s'\n", "ItemXX", cachelot_item_get_key(i));
        __test_cb_on_eviction_callback_succeded = false;
    }

    if (cachelot_item_get_valuelen(i) != strlen(__large_value) || strncmp(cachelot_item_get_value(i), __large_value, strlen(__large_value))) {
        printf("on_eviction_callback unexpected value: '%s' != '%s'\n", __large_value, cachelot_item_get_value(i));
        __test_cb_on_eviction_callback_succeded = false;
    }
}
bool test_on_eviction_callback(CachelotError * out_err) {
    const CachelotOptions smallCache = {
        .memory_limit = 1024u,
        .mem_page_size = 256u,
        .initial_dict_size = 4u,
        .enable_evictions = true,
    };
    CachelotPtr c = cachelot_init(smallCache, out_err);
    if (c == NULL) {
        print_cachelot_error("test on_eviction_callback: failed to create cache", out_err);
        return false;
    }
    cachelot_on_eviction_callback(c, &__test_cb_on_eviction_callback);
    bool ret = true;
    for (int i = 0; i < 100; ++i) {
        char theKey[8];
        sprintf(theKey, "Item%02d", i);
        CachelotItemPtr item = new_item(c, theKey, __large_value, out_err);
        if (item == NULL) {
            print_cachelot_error("test on_eviction_callback: failed to create item", out_err);
        }
        if (!cachelot_set(c, item, out_err) ) {
            print_cachelot_error("test on_eviction_callback: failed to set new item", out_err);
            ret = false; goto cleanup;
        }
        if (i == 80) {
            __at_this_point_callback_should_not_be_called = true;
            cachelot_on_eviction_callback(c, NULL);
        }
    }

    ret = __test_cb_on_eviction_callback_succeded;
    cleanup:
        cachelot_destroy(c);
    return ret;
}


int main() {
    printf("Testing ver. %s C API ....\n", cachelot_version());
    printf("memory_limit = %zu\n", options.memory_limit);
    printf("mem_page_size = %zu\n", options.mem_page_size);
    printf("initial_dict_size = %zu\n", options.initial_dict_size);
    printf("enable_evictions = %d\n\n", options.enable_evictions);
    CachelotError error_struct;
    CachelotError * err = &error_struct;
    CachelotPtr cache = cachelot_init(options, err);
    if (cache == NULL) {
        print_cachelot_error("Failed to create cache", err);
        return 1;
    }
    int ret = 0;
    if (! test_basic(cache, err)) {
        print_cachelot_error("basic tests failed", err);
        ret = 1;
        goto cleanup;
    }
    if (! test_arith(cache, err)) {
        print_cachelot_error("incr/decr tests failed", err);
        ret = 1;
        goto cleanup;
    }
    if (! test_expiration(cache, err)) {
        print_cachelot_error("expiration tests failed", err);
        ret = 1;
        goto cleanup;
    }
    if (! test_item_functions(cache, err)) {
        print_cachelot_error("expiration tests failed", err);
        ret = 1;
        goto cleanup;
    }
    if (! test_on_eviction_callback(err)) {
        print_cachelot_error("on eviction callback tests failed", err);
        ret = 1;
        goto cleanup;
    }
    printf("All tests passes\n");

cleanup:
    if (cache != NULL) {
        cachelot_destroy(cache);
    }

    return ret;
}

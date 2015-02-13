#include <cachelot/cachelot.h>
#include <cachelot/cache_async_api.h>

namespace cachelot { namespace cache {

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
        RequestArgs & operator= (const RequestArgs &) = delete;

        /// convert to the specific request type arguments
        template <class SpecificRequestArgs>
        SpecificRequestArgs & as() noexcept {
            static_assert(std::is_base_of<RequestArgs, SpecificRequestArgs>::value, "Invalid request class provided");
            return static_cast<SpecificRequestArgs &>(*this);
        }
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
        explicit DelRequestArgs(const bytes the_key, const hash_type the_hash, Callback the_callback) noexcept
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



} } // namespace cachelot::cache


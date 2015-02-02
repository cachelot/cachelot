#ifndef CACHELOT_CACHE_ASYNC_API_H_INCLUDED
#define CACHELOT_CACHE_ASYNC_API_H_INCLUDED

/// @ingroup cache
/// @{

namespace cachelot {
    
    namespace cache { namespace api {

        /// Request types
        struct GetRequest;      /// < `get`
        struct StoreRequest;    /// < `set` / `replace` / `add` / `prepend` / `append`
        struct DelRequest;      /// < `del`
        struct TouchRequest;    /// < `touch`
        struct CasRequest;      /// < `cas`
        struct IncDecRequest;   /// < `inc` / `dec`
        struct InternalRequest; /// < `stat` / `flush`

        /**
         * AsyncRequest is internally used by Storage to hold requests in a queue
         */
        struct AsyncRequest : public mpsc_queue<AsyncRequest>::node {
            /// type of the request
            const atom_type type;
            /// requested item key
            const bytes key;
            /// requested item hash
            const Storage::hash_type hash;

            /// constructor
            explicit AsyncRequest(const atom_type the_type, const bytes the_key, const Storage::hash_type the_hash)
            : type(the_type)
            , key(the_key)
            , hash(the_hash) {
            }

            // disallow copying
            AsyncRequest(const AsyncRequest &) = delete;
            AsyncRequest & operator= (const AsyncRequest &) = delete;

            /// virtual destructor
            virtual ~AsyncRequest() {}

            /// constructor
            template <class SpecificRequest>
            SpecificRequest & as() noexcept {
                static_assert(std::is_base_of<Storage::AsyncRequest, SpecificRequest>::value, "Invalid request class provided");
                return static_cast<SpecificRequest &>(*this);
            }

            /// utility function to create point in time from duration in seconds
            static Storage::expiration_time_type time_from_duration(chrono::seconds expire_after) {
                return expire_after == chrono::seconds(0) ? Storage::expiration_time_type::max() : Storage::clock_type::now() + expire_after;
            }
        };

        /// GetRequest is a specific AsyncRequest for get operation
        struct Storage::GetRequest : public Storage::AsyncRequest {
            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*found*/, bytes /*value*/, Storage::flags_type /*flags*/, Storage::cas_type /*cas_value*/)> callback;

            /// constructor
            template <typename Callback>
            explicit GetRequest(const bytes the_key, const Storage::hash_type the_hash, Callback the_callback)
            : Storage::AsyncRequest(atom("get"), the_key, the_hash)
            , callback(the_callback) {
            }
        };

        /// StoreRequest is a specific AsyncRequest for one of the store operations
        struct Storage::StoreRequest : public Storage::AsyncRequest {
            /// value of a stored item
            bytes value;

            /// opaque user defined bit flags
            Storage::flags_type flags;

            /// expiration time point
            Storage::expiration_time_type expiration_time;

            /// unique value for the `CAS` operation
            Storage::cas_type cas_value;

            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*stored*/)> callback;

            /// constructor
            template <typename Callback>
            explicit StoreRequest(const atom_type the_type, const bytes the_key, const Storage::hash_type the_hash,
                                  bytes the_value, Storage::flags_type the_flags, Storage::seconds expires_after,
                                  Storage::cas_type the_cas_value, Callback the_callback)
            : Storage::AsyncRequest(the_type, the_key, the_hash)
            , value(the_value)
            , flags(the_flags)
            , expiration_time(time_from_duration(expires_after))
            , callback(the_callback) {
            }
        };

        /// DelRequest is a specific AsyncRequest for del operation
        struct Storage::DelRequest : public Storage::AsyncRequest {
            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*deleted*/)> callback;

            /// constructor
            template <typename Callback>
            explicit DelRequest(const bytes the_key, const Storage::hash_type the_hash, Callback the_callback)
            : Storage::AsyncRequest(atom("del"), the_key, the_hash)
            , callback(the_callback) {
            }
        };
        
        struct Storage::TouchRequest : public Storage::AsyncRequest {
            /// expiration time point
            Storage::expiration_time_type expiration_time;
            
            /// callback to notify operation completion
            std::function<void (error_code /*error*/, bool /*touched*/)> callback;
            
            /// constructor
            template <typename Callback>
            explicit TouchRequest(const bytes the_key, const Storage::hash_type the_hash, Storage::seconds expires_after, Callback the_callback)
            : Storage::AsyncRequest(atom("touch"), the_key, the_hash)
            , expiration_time(time_from_duration(expires_after))
            , callback(the_callback) {
            }
        };

            
    } } // namespace cache::api

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_ASYNC_API_H_INCLUDED

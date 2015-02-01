#ifndef CACHELOT_CACHE_API_H_INCLUDED
#define CACHELOT_CACHE_API_H_INCLUDED

/// @ingroup cache
/// @{

namespace cachelot {
    
    namespace cache { namespace api {

        /// Request types
        struct GetRequest;      ///< `get`
        struct StoreRequest;    ///< `set` / `replace` / `add` / `prepend` / `append`
        struct DelRequest;      ///< `del`
        struct TouchRequest;    ///< `touch`
        struct CasRequest;      ///< `cas`
        struct IncDecRequest;   ///< `inc` / `dec`
        struct InternalRequest; ///< `stat` / `flush`

    } } // namespace cache::api

} // namespace cachelot

/// @}

#endif // CACHELOT_CACHE_API_H_INCLUDED

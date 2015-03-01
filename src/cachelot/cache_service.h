#ifndef CACHELOT_CACHE_CACHE_SERVICE_H_INCLUDED
#define CACHELOT_CACHE_CACHE_SERVICE_H_INCLUDED

#ifndef CACHELOT_CACHE_H_INCLUDED
#  include <cachelot/cache.h>
#endif
#ifndef CACHELOT_ACTOR_H_INCLUDED
#  include <cachelot/actor.h>
#endif



/// @ingroup cache
/// @{

namespace cachelot {

    namespace cache {

        /**
         * Actor based cache service wrapper
         *
         * Cache server speaks via Messages
         * -> `get` <key> <value> <serialization_type> <io_buffer>
         * <- `item` (item write into io_buffer)
         * <- `error` code
         *
         * TODO: document every command
         *
         * @see Cache
         */
        class CacheService : public Actor {
            // Underlying dictionary
            typedef dict<bytes, ItemPtr, std::equal_to<bytes>, ItemDictEntry, DictOptions> dict_type;
            typedef dict_type::iterator iterator;
        public:
            typedef dict_type::hash_type hash_type;
            typedef dict_type::size_type size_type;
        public:
            /// @copydoc Cache::Cache()
            explicit CacheService(ActorThread & cache_thread, size_t memory_size, size_t initial_dict_size);

            /// destructor
            ~CacheService();

        private:
            /// thread function
            bool main() noexcept;

        private:
            Cache m_cache;
        };


} } // namespace cachelot::cache

/// @}

#endif // CACHELOT_CACHE_CACHE_SERVICE_H_INCLUDED

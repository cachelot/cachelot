#ifndef CACHELOT_SETTINGS_H_INCLUDED
#define CACHELOT_SETTINGS_H_INCLUDED

/// @defgroup settings Program settings
/// @{

namespace cachelot {

    struct settings {
        size_t cache_memory_limit;
        size_t initial_hash_table_size;
        size_t initial_rcv_buffer_size;
        size_t initial_snd_buffer_size;
        size_t max_rcv_buffer_size;
        size_t max_snd_buffer_size;
        bool cache_cas_enabled;
        bool cache_eviction_enabled;
    };

    extern struct settings settings;

} // namespace cachelot

/// @}

#endif // CACHELOT_SETTINGS_H_INCLUDED

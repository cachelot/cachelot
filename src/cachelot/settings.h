#ifndef CACHELOT_SETTINGS_H_INCLUDED
#define CACHELOT_SETTINGS_H_INCLUDED

/// @defgroup settings Program settings
/// @{

namespace cachelot {

    struct settings {
        struct {
            size_t memory_limit = 64 * 1024 * 1024; // 64Mb
            size_t initial_hash_table_size = 1048576; // ~1M cells
            size_t max_value_size = 4 * 1024 * 1024; // 4Mb
            bool has_CAS = true;
            bool has_evictions = true;
        } cache;
        struct {
            size_t number_of_threads = 2;
            bool has_TCP = true;
            string TCP_interface = "localhost";
            uint16 TCP_port = 11211;
            bool hasUDP = true;
            tuple<string, string> UDP_interface = tuple<string, string>("localhost", "11212");
            bool has_unix_socket = false;
            string unix_socket = "/var/run/cachelot/cachelot.socket";
            uint32 unix_socket_access = 0700;
            size_t initial_rcv_buffer_size = 2048;
            size_t initial_snd_buffer_size = 2048;
            size_t max_rcv_buffer_size = 32*1024*1024;
            size_t max_snd_buffer_size = 32*1024*1024;
        } net;
    };

    extern struct settings settings;

} // namespace cachelot

/// @}

#endif // CACHELOT_SETTINGS_H_INCLUDED

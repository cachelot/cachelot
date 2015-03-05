#include <cachelot/cachelot.h>
#include <cachelot/proto_memcached_servers.h>
#include <cachelot/cache_service.h>
#include <cachelot/settings.h>

#include <iostream>
#include <signal.h>

using std::cerr;
using std::endl;
using namespace cachelot;

// Global io_service to access it from the signal handler
static bool killed = false;
net::io_service reactor;
std::unique_ptr<cache::CacheService> cache_svc;

void on_signal_terminate(int) {
    killed = true;
    reactor.stop();
    if (cache_svc) { cache_svc->stop(); }
}

void setup_signals() {
    signal(SIGTERM, &on_signal_terminate);
    signal(SIGKILL, &on_signal_terminate);
    signal(SIGINT, &on_signal_terminate);
    signal(SIGQUIT, &on_signal_terminate);
}




int main() {
    try {
        // Cache Service
        cache_svc.reset(new cache::CacheService(settings.cache.memory_limit,
                                                settings.cache.initial_hash_table_size));

        cache_svc->start();

        // Signal handlers
        setup_signals();

        // TCP
        std::unique_ptr<memcached::text_tcp_server> memcached_tcp_text = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp_text.reset(new memcached::text_tcp_server(reactor, *cache_svc));
            memcached_tcp_text->start(settings.net.TCP_port);
        }

        do {
            error_code ignore;
            reactor.run(ignore);
        } while(not killed);

        cache_svc->stop();
        cache_svc->join();

        cache_svc.release();

        return EXIT_SUCCESS;
    } catch (const std::exception & e) {
        cerr << "Exception caught: " << e.what() << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "Unknown exception caught" << endl;
        return EXIT_FAILURE;
    }
}

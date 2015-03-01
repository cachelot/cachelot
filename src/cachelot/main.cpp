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
net::io_service reactor;

void on_signal_terminate(int) {
    reactor.stop();
}

void setup_signals() {
    signal(SIGTERM, &on_signal_terminate);
    signal(SIGKILL, &on_signal_terminate);
    signal(SIGINT, &on_signal_terminate);
    signal(SIGQUIT, &on_signal_terminate);
}

int main() {
    try {
        std::unique_ptr<cache::AsyncCacheAPI> cache(new cache::AsyncCacheAPI(settings.cache.memory_limit, settings.cache.initial_hash_table_size));
        std::vector<std::thread> reactor_thread_pool;

        // TCP
        std::unique_ptr<memcached::text_tcp_server> memcached_tcp_text;
        if (settings.net.has_TCP) {
            memcached_tcp_text.reset(new memcached::text_tcp_server(reactor, *cache));
            string host, service;
            memcached_tcp_text->start(settings.net.TCP_port);
        }

        // Threads
        for (unsigned thread_no = 0; thread_no < settings.net.number_of_threads; ++thread_no) {
            reactor_thread_pool.emplace_back(std::move(std::thread( [=]() { reactor.run(); } )));
        }

        // Signal handlers
        setup_signals();

        // All done wait complete
        for (auto & th : reactor_thread_pool) {
            th.join();
        }

        return EXIT_SUCCESS;
    } catch (const std::exception & e) {
        cerr << "Exception caught: " << e.what() << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "Unknown exception caught" << endl;
        return EXIT_FAILURE;
    }
}

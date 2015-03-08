#include <cachelot/cachelot.h>
#include <cachelot/proto_memcached_servers.h>
#include <cachelot/thread_safe_cache.h>
#include <cachelot/settings.h>

#include <iostream>

#include <signal.h>

using std::cerr;
using std::endl;
using namespace cachelot;

namespace  {
    // Global io_service to access it from the signal handler
    static bool killed = false;
    // Reactor
    static net::io_service reactor;

    void on_signal_terminate(int) {
        killed = true;
        reactor.stop();
    }

    void setup_signals() {
        signal(SIGTERM, &on_signal_terminate);
        signal(SIGKILL, &on_signal_terminate);
        signal(SIGINT, &on_signal_terminate);
        signal(SIGQUIT, &on_signal_terminate);
    }
}


int main() {
    try {
        // Cache Service
        std::unique_ptr<cache::ThreadSafeCache> the_cache(new cache::ThreadSafeCache(settings.cache.memory_limit,
                                                                                     settings.cache.initial_hash_table_size));
        // Signal handlers
        setup_signals();

        // TCP
        std::unique_ptr<memcached::text_tcp_server> memcached_tcp_text = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp_text.reset(new memcached::text_tcp_server(reactor, *the_cache));
            memcached_tcp_text->start(settings.net.TCP_port);
        }

        std::thread t1([=]() { reactor.run(); });
        std::thread t2([=]() { reactor.run(); });


        do {
            error_code ignore;
            reactor.run(ignore);
        } while(not killed);

        // stop everything
        reactor.stop();

        t1.join();
        t2.join();

        return EXIT_SUCCESS;
    } catch (const std::exception & e) {
        cerr << "Exception caught: " << e.what() << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "Unknown exception caught" << endl;
        return EXIT_FAILURE;
    }
}

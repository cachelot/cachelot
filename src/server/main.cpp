#include <cachelot/common.h>
#include <server/memcached/servers.h>
#include <cachelot/cache.h>
#include <server/settings.h>
#include <cachelot/stats.h>

#include <iostream>
#include <boost/program_options.hpp>
#include <signal.h>

using std::cerr;
using std::endl;
using namespace cachelot;
using std::string;
namespace po = boost::program_options;

namespace  {
    // Global io_service to access it from the signal handler
    static bool killed = false;
    // Reactor
    static net::io_service reactor;

    void on_signal_terminate(int) {
        killed = true;
        reactor.stop();
    }

    void on_signal_print_stats(int) {
        try { PrintStats(); } catch (...) {}
    }

    void setup_signals() {
        signal(SIGTERM, &on_signal_terminate);
        signal(SIGKILL, &on_signal_terminate);
        signal(SIGINT, &on_signal_terminate);
        signal(SIGQUIT, &on_signal_terminate);
        signal(SIGUSR1, &on_signal_print_stats);
    }

    /// Command line arguments parser
    int parse_cmdline(int argc, const char * const argv[]) {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("tcp-port,p", po::value<uint16>()->default_value(11211),   "TCP port number to listen on")
            ("udp-port,U", po::value<uint16>()->default_value(11211),   "UDP port number to listen on (0 is off)")
            ("socket,s",   po::value<string>(),                         "UNIX socket path to listen on")
            ("socket_access,a", po::value<unsigned>(),                  "access mask for UNIX socket, in octal (default: 0700)")
            ("listen,l",   po::value<std::vector<string>>(),            "interface to listen on (default: INADDR_ANY, all addresses)\n"
                                                                        "<arg> may be specified as host:port. If you don't specify a port number,"
                                                                        "the value you specified with -p or -U is used."
                                                                        "You may specify multiple addresses separated by comma or by using -l multiple times")
            ("daemon,d",   po::bool_switch()->default_value(false),     "run as a daemon")
            ("OUM-error,M", po::bool_switch()->default_value(false),    "return error on memory exhausted (rather than removing items)")
            ("no-cas,C",   po::bool_switch()->default_value(false),     "disable use of CAS (memory economy)")
            ("memory,m",   po::value<unsigned>(),                       "max memory to use for items in megabytes (default: 64 MB)")

        ;

        po::variables_map varmap;
        po::store(po::parse_command_line(argc, argv, desc), varmap);
        // TODO: May throw
        po::notify(varmap);

        if (varmap.count("help")) {
            cerr << desc << "\n";
            return EXIT_FAILURE;
        }
        if (varmap.count("tcp-port") == 1) {
            settings.net.TCP_port = varmap["tcp-port"].as<uint16>();
        }
        if (varmap.count("udp-port") == 1) {
            settings.net.UDP_port = varmap["udp-port"].as<uint16>();
            if (settings.net.UDP_port != 0) {
                settings.net.has_UDP = true;
            } else {
                settings.net.has_UDP = false;
            }
        }
        if (varmap.count("socket") == 1) {
            settings.net.unix_socket = varmap["socket"].as<string>();
            settings.net.has_unix_socket = true;
        }
        settings.cache.has_evictions = not varmap["OUM-error"].as<bool>();
        settings.cache.has_CAS = not varmap["no-cas"].as<bool>();

        return 0;
    }
}


int main(int argc, char * argv[]) {
    try {
        if (parse_cmdline(argc, argv) != 0) {
            return EXIT_FAILURE;
        }
        // Cache Service
        std::unique_ptr<cache::Cache> the_cache(new cache::Cache(settings.cache.memory_limit, settings.cache.initial_hash_table_size, settings.cache.has_evictions));
        // Signal handlers
        setup_signals();

        // TCP
        std::unique_ptr<memcached::text_tcp_server> memcached_tcp_text = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp_text.reset(new memcached::text_tcp_server(reactor, *the_cache));
            memcached_tcp_text->start(settings.net.TCP_port);
        }

        // Unix socket
        std::unique_ptr<memcached::text_unix_stream_server> memcached_unix_stream_text = nullptr;
        if (settings.net.has_unix_socket) {
            memcached_unix_stream_text.reset(new memcached::text_unix_stream_server(reactor, *the_cache));
            memcached_unix_stream_text->start(settings.net.unix_socket);
        }

        error_code error;
        do {
            reactor.run(error);
        } while(not killed && not error);

        if (memcached_tcp_text) {
            memcached_tcp_text->stop();
        }
        if (memcached_unix_stream_text) {
            memcached_unix_stream_text->stop();
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

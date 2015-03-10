#include <cachelot/cachelot.h>
#include <cachelot/proto_memcached_servers.h>
#include <cachelot/cache.h>
#include <cachelot/settings.h>

#include <iostream>
#include <boost/program_options.hpp>

#include <signal.h>

using std::cerr;
using std::endl;
using namespace cachelot;
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

    void setup_signals() {
        signal(SIGTERM, &on_signal_terminate);
        signal(SIGKILL, &on_signal_terminate);
        signal(SIGINT, &on_signal_terminate);
        signal(SIGQUIT, &on_signal_terminate);
    }

    int parse_cmdline(int argc, const char * const argv[]) {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("tcp-port,p", po::value<uint16>(), "TCP port number to listen on (default: 11211)")
            ("udp-port,U", po::value<uint16>(), "UDP port number to listen on (default: 11211, 0 is off)")
        ;

        po::variables_map varmap;
        po::store(po::parse_command_line(argc, argv, desc), varmap);
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
        }
        return 0;
    }
}


int main(int argc, char * argv[]) {
    try {
        if (parse_cmdline(argc, argv) != 0) {
            return EXIT_FAILURE;
        }
        // Cache Service
        std::unique_ptr<cache::Cache> the_cache(new cache::Cache());
        // Signal handlers
        setup_signals();

        // TCP
        std::unique_ptr<memcached::text_tcp_server> memcached_tcp_text = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp_text.reset(new memcached::text_tcp_server(reactor, *the_cache));
            memcached_tcp_text->start(settings.net.TCP_port);
        }

        error_code error;
        do {
            reactor.run(error);
        } while(not killed && not error);

        if (memcached_tcp_text) {
            memcached_tcp_text->stop();
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

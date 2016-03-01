#include <cachelot/common.h>
#include <cachelot/cache.h>
#include <cachelot/stats.h>
#include <server/settings.h>
#include <server/memcached/conversation.h>

#include <iostream>
#include <boost/program_options.hpp>
#include <signal.h>

using std::cerr;
using std::endl;
using namespace cachelot;
using std::string;
namespace po = boost::program_options;

namespace  {

    /// Command line arguments parser
    int parse_cmdline(int argc, const char * const argv[]) {
        po::options_description desc("Cachelot is lightning fast in-memory caching system\n"
                                     "visit http://cachelot.io for support");
        desc.add_options()
            ("help,h",                                                  "Produce this help message")
            ("version,V",                                               "Print version and exit")
            ("tcp-port,p", po::value<uint16>()->default_value(11211),   "TCP port number to listen on (0 to disable TCP)")
            ("udp-port,U", po::value<uint16>()->default_value(11211),   "UDP port number to listen on (0 to disable UDP)")
            ("socket,s",   po::value<string>(),                         "Unix socket path to listen on (disabled by default)")
            ("socket_access,a", po::value<unsigned>(),                  "Access mask for the unix socket, in octal (default: 0700)")
            ("listen,l",   po::value<std::vector<string>>(),            "Interface to listen on (default: INADDR_ANY - all addresses)\n"
                                                                        "<arg> may be specified as host:port. If you don't specify a port number,"
                                                                        "the value you specified with -p or -U is used."
                                                                        "You may specify multiple addresses separated by comma or by using -l multiple times")
            ("daemon,d",   po::bool_switch()->default_value(false),     "Run as a daemon")
            ("OUM-error,M", po::bool_switch()->default_value(false),    "Return error on memory exhausted (rather than removing items)")
            ("no-cas,C",   po::bool_switch()->default_value(false),     "Disable use of CAS (memory economy)")
            ("memory,m",   po::value<unsigned>(),                       "Max memory to use for items in megabytes (default: 64 MB)")
        ;

        po::variables_map varmap;
        po::store(po::parse_command_line(argc, argv, desc), varmap);
        // TODO: May throw
        po::notify(varmap);

        if (varmap.count("help")) {
            cerr << desc << endl;
            return EXIT_FAILURE;
        }
        if (varmap.count("version")) {
            cerr << CACHELOT_VERSION_FULL << endl;
            cerr << CACHELOT_VERSION_NOTICE << endl;
            return EXIT_FAILURE;
        }
        if (varmap.count("tcp-port") == 1) {
            settings.net.TCP_port = varmap["tcp-port"].as<uint16>();
        }
        settings.net.has_TCP = settings.net.TCP_port != 0;
        if (varmap.count("udp-port") == 1) {
            settings.net.UDP_port = varmap["udp-port"].as<uint16>();
        }
        settings.net.has_UDP = settings.net.UDP_port != 0;
        if (varmap.count("socket") == 1) {
            settings.net.unix_socket = varmap["socket"].as<string>();
        }
        settings.net.has_unix_socket = not settings.net.unix_socket.empty();
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
        std::unique_ptr<cache::Cache> the_cache(new cache::Cache(settings.cache.memory_limit,
                                                                 settings.cache.page_size,
                                                                 settings.cache.initial_hash_table_size,
                                                                 settings.cache.has_evictions));
        // Reactor service
        net::io_service reactor;

        // TCP
        std::unique_ptr<memcached::TcpServer> memcached_tcp = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp.reset(new memcached::TcpServer(*the_cache, reactor));
            net::tcp::endpoint bind_addr(net::ip::address_v4::any(), settings.net.TCP_port);
            memcached_tcp->start(bind_addr);
        }

        // Unix local socket
        std::unique_ptr<memcached::UnixSocketServer> memcached_unix_socket = nullptr;
        if (settings.net.has_unix_socket) {
            memcached_unix_socket.reset(new memcached::UnixSocketServer(*the_cache, reactor));
            memcached_unix_socket->start(settings.net.unix_socket);
        }

        // UDP
        std::unique_ptr<memcached::UdpServer> memcached_udp = nullptr;
        if (settings.net.has_UDP) {
            memcached_udp.reset(new memcached::UdpServer(*the_cache, reactor));
            net::udp::endpoint bind_addr(net::ip::address_v4::any(), settings.net.UDP_port);
            memcached_udp->start(bind_addr);
        }

        // Signal handlers
        boost::asio::signal_set signals(reactor);
        signals.add(SIGTERM);
        signals.add(SIGINT);
        signals.add(SIGQUIT);
        signals.add(SIGUSR1);
        signals.async_wait([&reactor](const error_code& error, int signal_number) {
            if (error) { return; }
            switch (signal_number) {
            case SIGUSR1:
                PrintStats();
                break;
            default:
                reactor.stop();
            }
        });

        // Run reactor loop
        do {
            reactor.run();
        } while (not reactor.stopped());


        return EXIT_SUCCESS;
    } catch (const std::exception & e) {
        cerr << "Exception caught: " << e.what() << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "Unknown exception caught" << endl;
        return EXIT_FAILURE;
    }
}

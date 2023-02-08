//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file

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

    struct invalid_configuration : public std::invalid_argument {
        explicit invalid_configuration(const char * msg) : std::invalid_argument(msg) {}
        explicit invalid_configuration(const string & msg) : std::invalid_argument(msg) {}
    };

    // wrapper for the 'memory' and 'page' options with validator
    struct po_memory { size_t n; };
    void validate(boost::any& arg, const std::vector<std::string>& values, po_memory *, int) {
        po::validators::check_first_occurrence(arg);
        auto s = po::validators::get_single_string(values);
        size_t units;
        switch (s[s.length()-1]) {
        case 'K':
            units = Kilobyte;
            s.pop_back();
            break;
        case 'M':
            units = Megabyte;
            s.pop_back();
            break;
        case 'G':
            units = Gigabyte;
            s.pop_back();
            break;
        default:
            units = Megabyte;
        }
        size_t memory_amount;
        try {
            memory_amount = boost::lexical_cast<size_t>(s) * units;
        } catch (const boost::bad_lexical_cast &) {
            throw po::invalid_option_value(s);
        }
        if (memory_amount == 0) {
            throw invalid_configuration("zero memory amount");
        }
        if (not ispow2(memory_amount)) {
            throw invalid_configuration("memory amount must be power of 2");
        }
        arg = boost::any(po_memory{memory_amount});
    }

    /// Command line arguments parser
    int parse_cmdline(int argc, const char * const argv[]) {
        po::options_description desc("Cachelot is lightning fast in-memory caching system\n"
                                     "visit http://www.cachelot.io");
        desc.add_options()
            ("help,h",                              "Produce this help message")
            ("version,V",                           "Print version and exit")
            ("tcp-port,p",  po::value<uint16>(),    "TCP port (0 to disable TCP)")
            ("udp-port,U",  po::value<uint16>(),    "UDP port (0 to disable UDP)")
            ("socket,s",    po::value<string>(),    "Unix domain socket path (disabled by default)")
            ("socket_access,a", po::value<unsigned>(), "Access mask for the unix socket, in octal (default: 0700)")
            ("listen,l",    po::value<std::vector<string>>(),
                                                    "Interface to listen on (default: INADDR_ANY - all addresses)\n"
                                                    "<arg> may be specified as hostname[:port]. If port number is not specified,"
                                                    "it will be taken from -p or -U or default."
                                                    "You may specify multiple addresses separated by comma or by using -l multiple times")
            ("daemon,d",    po::bool_switch(),      "Run as a daemon")
            ("oum-error,M", po::bool_switch(),      "Return error when out of memory (rather than removing items)")
            ("no-cas,C",    po::bool_switch(),      "Disable use of CAS (memory economy)")
            ("memory,m",    po::value<po_memory>(), "Max memory to use for items storage in megabytes (must be power of 2)"
                                                    "You may specify one of the suffixes (K,M,G) to use different units"
                                                    "For instance, -m 8G means 8 Gigabytes of RAM")
            ("page,P",      po::value<po_memory>(), "Page size in megabytes (must be power of 2)"
                                                    "You may specify one of the suffixes (K,M,G) to use different units"
                                                    "Lesser pages leads to more accurate evictions, although page size affects maximal item size")
            ("hashtable,H", po::value<size_t>(),    "Initial hash table size (default 64K)")
        ;

        po::variables_map varmap;
        po::store(po::parse_command_line(argc, argv, desc), varmap);

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
        if (varmap.count("tcp-port")) {
            settings.net.TCP_port = varmap["tcp-port"].as<uint16>();
        }
        settings.net.has_TCP = settings.net.TCP_port != 0;
        if (varmap.count("udp-port")) {
            settings.net.UDP_port = varmap["udp-port"].as<uint16>();
        }
        settings.net.has_UDP = settings.net.UDP_port != 0;
        if (varmap.count("socket")) {
            settings.net.unix_socket = varmap["socket"].as<string>();
        }
        settings.net.has_unix_socket = not settings.net.unix_socket.empty();
        settings.cache.has_evictions = not varmap["oum-error"].as<bool>();
        settings.cache.has_CAS = not varmap["no-cas"].as<bool>();
        if (varmap.count("memory")) {
            settings.cache.memory_limit = varmap["memory"].as<po_memory>().n;
        }
        if (varmap.count("page")) {
            settings.cache.page_size = varmap["page"].as<po_memory>().n;
        }
        if (settings.cache.memory_limit < (settings.cache.page_size * 4)) {
            throw invalid_configuration("There must be at least 4 pages");
        }
        if (settings.cache.page_size > 2*Gigabyte) {
            throw invalid_configuration("Maximal page size is 2Gb");
        }
        if (varmap.count("hashtable")) {
            settings.cache.initial_hash_table_size = varmap["hashtable"].as<size_t>();
        }
        if (not ispow2(settings.cache.initial_hash_table_size)) {
            throw invalid_configuration("the argument for option '--hashtable' must be power of 2");
        }
        return EXIT_SUCCESS;
    }
}


int main(int argc, char * argv[]) {
    try {
        if (parse_cmdline(argc, argv) != 0) {
            return EXIT_FAILURE;
        }
        // Cache Service
        auto the_cache = cache::Cache::Create(settings.cache.memory_limit,
                                              settings.cache.page_size,
                                              settings.cache.initial_hash_table_size,
                                              settings.cache.has_evictions);
        // Reactor service
        net::io_service reactor;

        // TCP
        std::unique_ptr<memcached::TcpServer> memcached_tcp = nullptr;
        if (settings.net.has_TCP) {
            memcached_tcp.reset(new memcached::TcpServer(the_cache, reactor));
            net::tcp::endpoint bind_addr(net::ip::address_v4::any(), settings.net.TCP_port);
            memcached_tcp->start(bind_addr);
        }

        // Unix local socket
        std::unique_ptr<memcached::UnixSocketServer> memcached_unix_socket = nullptr;
        if (settings.net.has_unix_socket) {
            memcached_unix_socket.reset(new memcached::UnixSocketServer(the_cache, reactor));
            memcached_unix_socket->start(settings.net.unix_socket);
        }

        // UDP
        std::unique_ptr<memcached::UdpServer> memcached_udp = nullptr;
        if (settings.net.has_UDP) {
            memcached_udp.reset(new memcached::UdpServer(the_cache, reactor));
            net::udp::endpoint bind_addr(net::ip::address_v4::any(), settings.net.UDP_port);
            memcached_udp->start(bind_addr);
        }

        // Signal handlers
        boost::asio::signal_set signals(reactor);
        signals.add(SIGTERM);
        signals.add(SIGINT);
#if !defined(_MSC_VER)
        signals.add(SIGQUIT);
        signals.add(SIGUSR1);
#endif
        signals.async_wait([&reactor](const error_code& error, int signal_number) {
            if (error) { return; }
            switch (signal_number) {
#if !defined(_MSC_VER)
            case SIGUSR1:
                PrintStats();
                break;
#endif
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
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "Unknown internal error!" << endl;
        return EXIT_FAILURE;
    }
}

<a href="https://travis-ci.org/cachelot/cachelot/"><img src="https://travis-ci.org/cachelot/cachelot.svg?branch=master"/></a>

#### Build failed because of [gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61693)
100% tests pass on Clang build


# What is Cachelot Library #
If your application needs an LRU cache that works at the speed of light. That's what the Cachelot library is.

The library works within a fixed amount of memory. No garbadge collector. Small metadata, near perfect memory utilization.

Besides memory management, Cachelot ensures smooth responsiveness, without any "gaps" for both read and write operations.

Cachelot can work as a consistent cache, returning an error when out of memory or evicting old items to free space for new ones.

The code is written in C++ and it is highly CPU-optimized. You can use cachelot on platforms where resources are limited, like IoT devices or handheld.

All this allows you to store and access three million items per second (depending on the CPU cache size). Maybe 3MOPs doesn't sound like such a large number, but it means ~333 nanoseconds are spent on a single operation, while RAM reference cost is at [~100 nanoseconds](http://www.eecs.berkeley.edu/~rcs/research/interactive_latency.html).
Only 3 RAM reference per request, can you compete with that?

There are benchmarks inside of repo, we encourage you to try them for yourself.

It is possible to create bindings and use cachelot from your programming language of choice: Python, Go, Java, etc.

# What is Cachelot Distributed Cache Server #
Think of [Memcached](http://memcached.org) (Cachelot server is Memcached-compatible) but while the former aims to serve hundreds of simultaneous connections, achieving maximum bandwidth, the latter serves tens of them, achieving maximal hardware utilization.

Cachelot is single-threaded, so you can run it while consuming fewer resources; it also allows to store much more items in the same amount of RAM (up to 25% more, depending on the data and store patterns)

And yet, single Cachelot instance is [faster than Memcached](http://cachelot.io/index.html#benchmarks) while serving a certain number of connections. Never the less, you're free to scale up horizontally by running more instances.

The easiest way to play with cachelot is to run Docker container

    $ docker run --net=host cachelot/cachelot

Then you can connect to the port 11211 and speak memcached protocol

    $ telnet localhost 11211
    >set test 0 0 16
    >Hello, cachelot!
    STORED
    >get test
    VALUE test 0 16
    Hello, cachelot!
    END
    >quit

* * *

### Attention ###
Cachelot is in beta stage, don't use it in production systems.

## How To Build ##
Cachelot has been proven to work on Alpine Linux, CentOS 7, Ubuntu Trusty, MacOS and BSD Family.
Windows build is upcoming.

### Prerequisites ###

 * C++11 capable compiler
 * [Boost libraries](http://boost.org/) either includes / libraries must be at the standard locations or ${BOOST_ROOT} environment variable must point to the libraries location. Please note that cachelot links with boost statically
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build ###

Clone source code repository:

    $ git clone https://github.com/cachelot/cachelot.git

Next

    $ cd cachelot

Generate project files for your favorite IDE or Makefile by running `cmake -G "{generator}"` in main project directory.

For example:

    $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && make

or

    $ cmake -G "Xcode"

It's possible to configure the build by passing parameters to cmake:

Build with IDE or by running make in root project directory. All binaries (main executable, unit tests, etc.) will be in ./bin/`build_type` directory.


 * `-DCMAKE_BUILD_TYPE` to choose build type.
   Must be one of:
     - `Debug` - enable debug messages, assertions and disable optimization
     - `Release` - release build
     - `RelWithDebInfo` - release build with debug information enabled
     - `AddressSanitizer` - special build to run under [Address Sanitizer](https://code.google.com/p/address-sanitizer/) (compiler support and libasan required)

Build with IDE or by running `make` in the root project directory.

### Run tests or benchmarks ###
All binaries (main executable, unit tests, etc.) will be in `bin/{build_type}`.

### Visit the web site for more information ###
[www.cachelot.io](http://www.cachelot.io/)

### Follow Cachelot in social media ###
Be first to know about the new features

 * Twitter: [@cachelot_io](https://twitter.com/cachelot_io)
 * Facebook: [cachelot.io](https://facebook.com/cachelot.io)
 * Google+: [+CachelotIo_cache](https://plus.google.com/+CachelotIo_cache)

* * *

## License ##
Cachelot is free and open source.
Distributed under the terms of [Simplified BSD License](http://cachelot.io/license.txt)

## Credits ##
 * [boost C++ libraries](http://www.boost.org)
 * [TLSF](http://www.gii.upv.es/tlsf/)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)
 * Thanks to all the open source community


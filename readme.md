<a href="https://travis-ci.org/cachelot/cachelot/"><img src="https://travis-ci.org/cachelot/cachelot.svg?branch=master"/></a>

# What is Cachelot Library #
If your application needs an LRU cache that works at the speed of light. That's what the Cachelot library is.

The library works with a fixed pre-allocated memory. You tell the memory size and LRU cache is ready.

Small metadata, up to 98% memory utilization.

Besides memory management, Cachelot ensures smooth responsiveness, without any "gaps" for both read and write operations.

Cachelot can work as a consistent cache, returning an error when out of memory or evicting old items to free space for new ones.

The code is highly optimized C++. You can use cachelot on platforms where resources are limited, like IoT devices or handheld; as well, as on servers with tons of RAM.

All this allows you to store and access three million items per second (depending on the CPU cache size). Maybe 3MOPs doesn't sound like such a large number, but it means ~333 nanoseconds are spent on a single operation, while RAM reference cost is at [~100 nanoseconds](http://www.eecs.berkeley.edu/~rcs/research/interactive_latency.html).
Only 3 RAM reference per request, can you compete with that?

There are benchmarks inside of repo; we encourage you to try them for yourself.

It is possible to create bindings and use cachelot from your programming language of choice: Python, Go, Java, etc.

# What is Cachelot Distributed Cache Server #
Think of [Memcached](http://memcached.org) but Cachelot far better utilizes RAM so you can store more items in the same amount of memory. Also Cachelot is faster in terms of latency.
[See benchmarks](http://cachelot.io/index.html#benchmarks) on the cachelot web site.

Cachelot is single-threaded. It can scale to 1024 cores, and run even on battery-powered devices.

Cachelot supports TCP, UDP, and Unix sockets.

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

## Cross-platform ##
Cachelot is tested on Alpine Linux (Docker), CentOS 7, Ubuntu Trusty and MacOS.

32bit ARM and x86-64 supported.

Windows build is upcoming.

## How To Hack ##

### Prerequisites ###

 * C++11 capable compiler
 * [Boost libraries](http://boost.org/)
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build ###

Clone source code repository:

    $ git clone https://github.com/cachelot/cachelot.git

Next

    $ cd cachelot

Generate project files for your favorite IDE or Makefile by running `cmake -G "{target}"` in the cachelot root directory.

For example:

    $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && make

or

    $ cmake -G "Xcode"

There are several variants of build:

`-DCMAKE_BUILD_TYPE`  must be one of the:
     - `Debug` - enable debug messages, assertions and disable optimization
     - `Release` - release build
     - `RelWithDebInfo` - release build with debug information enabled
     - `MinSizeRel` - minimal size release
     - `AddressSanitizer` - special build to run under [Address Sanitizer](https://code.google.com/p/address-sanitizer/) (compiler support and libasan required)

### Run tests or benchmarks ###
All binaries (main executable, unit tests, etc.) will be in `bin/{build_type}`.

### Subscribe to Cachelot blog ###
[cachelot.io/blog](http://cachelot.io/blog/)

### Follow Cachelot in social media ###
Be first to know about the new features

 * Twitter: [@cachelot_io](https://twitter.com/cachelot_io)
 * Facebook: [cachelot.io](https://facebook.com/cachelot.io)

* * *

## License ##
Cachelot is free and open source.
Distributed under the terms of [Simplified BSD License](http://cachelot.io/license.txt)

## Credits ##
 * [boost C++ libraries](http://www.boost.org)
 * [TLSF](http://www.gii.upv.es/tlsf/)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)
 * Thanks to all the open source community


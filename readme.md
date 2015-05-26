# What is Cachelot Library #
If your application needs to cache data and retrieve it from the cache at the speed of light, the only way to do so is to store data right into the application's memory. That's what the Cachelot library is.

The library works within a fixed amount of memory. In fact, the user is responsible for providing memory range for the cache. For instance, it can be a battery-protected or mapped memory.

Besides memory management, Cachelot ensures smooth responsiveness, without any "gaps" for both read and write operations.

Cachelot can work as a consistent cache, returning an error when out of memory or evicting old items to free space for new ones.

The code is written in C++ in a hardware-friendly manner.

All this allows you to store and access three million items per second (depending on the CPU cache size). Maybe 3MOPs doesn't sound like such a large number, but it means ~333 nanoseconds are spent on a single operation, while RAM reference cost is at [~100 nanoseconds](http://www.eecs.berkeley.edu/~rcs/research/interactive_latency.html).

# What is Cachelot Distributed Cache Server #
Think of [Memcached](http://memcached.org) (Cachelot server is Memcached-compatible) but while the former aims to serve hundreds of simultaneous connections, achieving maximum bandwidth, the latter serves tens of them, achieving minimal latency.

Cachelot is single-threaded, so you can run it while consuming fewer resources (getting cheaper VPS hosts can lead to the huge savings); it also stores less metadata per Item, and thus better utilizes precious RAM.

And yet, single Cachelot instance is [faster than Memcached](http://cachelot.io/index.html#benchmarks) while serving a certain number of connections. Never the less, you're free to scale up horizontally by running more instances.

* * *

### Attention ###
Cachelot is in early-alpha stage, don't use it in production systems

## How To Build ##
Cachelot has been proven to work on Linux, MacOS and BSD Family.
Windows build is upcoming.

### Prerequisites ###

 * C++11 capable compiler
 * [Boost libraries](http://boost.org/) either includes / libraries must be at the standard locations or ${BOOST_ROOT} environment variable must point to the libraries location. Please note that cachelot links with boost statically
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build ###

Clone source code repository on Bitbucket:

    $ hg clone https://bitbucket.org/cachelot/cachelot

or if you prefer GitHub:

    $ git clone https://github.com/cachelot/cachelot.git

Next

    $ cd cachelot

Generate project files for your favorite IDE or Makefile by running `cmake -G "{generator}"` in main project directory.

For example:

    $ cmake -G "Unix Makefiles"

or

    $ cmake -G "Xcode"

It's possible to configure the build by passing parameters to cmake:

Build with IDE or by running make in root project directory. All binaries (main executable, unit tests, etc.) will be in ./bin/<build_type> directory.


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

* * *

## Upcoming Features ##
 Cachelot is still 'work-in-progress' project

 * Some Memcached commands not implemented yet (~~`cas`~~, `touch`, `append`, `prepend`, `stats`)
 * Windows build is broken
 * There is no UDP and binary protocol support
 * Cachelot library doesn't have proper build packaging and has no multi-threaded interface
 * Documentation and examples
 * Really neat features, and more...

## License ##
Cachelot is free and open source. 
Distributed under the terms of [Simplified BSD License](http://cachelot.io/license.txt)

## Credits ##
 * [boost C++ libraries](http://www.boost.org)
 * [TLSF](http://www.gii.upv.es/tlsf/)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)
 * Thanks to all the open source community


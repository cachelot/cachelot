# What is Cachelot Library #
If your application needs to cache some data and then retrieve it from the cache with a speed of light, the only way is to store this data right into application's memory. That's what Cachelot library is.
One library with a simple interface. All you need to provide is a fixed amount of memory for the cache, and you will be able to access or store 3 million of items per second (depending on the CPU cache size). 

Maybe 3MOps doesn't sound like such a big number, but it does mean ~333 nanosecond spent on a single operation while RAM reference costs [~100 nanoseconds](http://www.eecs.berkeley.edu/~rcs/research/interactive_latency.html).

Cachelot can work as a consistent cache, returning an error when out of memory or evicting old items to store the new ones.

# What is Cachelot Distributed Cache Server #
Think Memcached (Cachelot is Memcached compatible). Though, while original [Memcached](http://memcached.org) aims to serve thousands of connections (bandwidth), Cachelot serves tens of those, but with a higher RPS rate (latency).

Another good news: Cachelot is a single-threaded application, so you can run it consuming fewer resources (for instance, cheaper VPS servers can be a huge saving).

And yet single Cachelot instance is faster than Memcached while serving a certain number of connections. Never the less, you're free to scale up horizontally by running more instances.

* * *

## How To Build ##
Currently, Windows build is broken. Cachelot proved to work on Linux and Mac OS (probably on BSD too)

### Prerequisites ###

 * C++11 capable compiler
 * [Boost libraries](http://boost.org/) either includes / libraries must be at the standard locations or ${BOOST_ROOT} environment variable must point to the libraries location. Please note that cachelot links with boost statically
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build ###

Clone source code repository

    $ hg clone http://dev.cachelot.io/cachelot/cachelot
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
     - `AddressSanitizer` - special build to run under [Address Sanitizer](https://code.google.com/p/address-sanitizer/) (compiler support required)

Build with IDE or by running `make` in root project directory.

### Run tests or benchmarks ###
All binaries (main executable, unit tests, etc.) will be in `bin/{build_type}`.

* * *

## Known Issues ##
 Cachelot is still in 'work in progress' state

 * Some Memcached commands not implemented yet (`cas`, `touch`, `append`, `prepend`, `stats`)
 * Windows build is broken
 * There is no UDP and binary protocol support
 * Cachelot library doesn't have proper build packaging and has no multi-threaded interface

## License ##
Cachelot distributed under the terms of [Simplified BSD License](http://opensource.org/licenses/BSD-2-Clause)

## Credits ##
 * [boost C++ libraries](http://www.boost.org)
 * [TLSF](http://www.gii.upv.es/tlsf/)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)
 * Thanks to all open source community
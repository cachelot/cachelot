# What is Cachelot Library #
If your application needs to cache some data and retrieve it from the cache with a speed of light, the easiest way is to store data right into application's memory. That's what cachelot library is. One library with the simple interface. All you need is to provide fixed amount of memory for the cache and access or store 3 millions of items per second (depending on CPU cache size).<br/>
Maybe 3MOps doesn't sound like too big number, but it means ~333 nanosecond spent on a single operation while RAM reference costs [~100 nanoseconds](http://www.eecs.berkeley.edu/~rcs/research/interactive_latency.html).<br/>
Cachelot can work as a consistent cache returning an error when out of memory or evict old items to store new.

# What is Cachelot Distributed Cache Server #
Think Memcached (it is Memcached compatible) but while Memcached aims to serve hundreds of thousands of connections (bandwidth) Cachelot serves tens of them with higher RPS rate (latency). Additionally cachelot is single-threaded application, so you can run it consuming fewer resources (cheaper VPS servers can save a lot). 
And yet cachelot is faster (up to 16% in terms of RPS, depending on a test setup).


## How To Build ##
Currently, Windows build is broken. Cachelot proved to work on Linux and Mac OS (probably on BSD too)

### Prerequisites ###

 * C++11 capable compiler
 * [Boost libraries](http://boost.org/) either includes / libraries must be at the standard locations or ${BOOST_ROOT} environment variable must point to the libraries location. Please note that cachelot links with boost statically
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build ###

Clone source code repository

    $ hg clone https://bitbucket.org/cachelot/cachelot
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
     - Debug - enable debug messages, assertions and disable optimization
     - Release - release build
     - RelWithDebInfo - release build with debug information enabled
     - AddressSanitizer - special build to run under [Address Sanitizer](https://code.google.com/p/address-sanitizer/) (compiler support required)

Build with IDE or by running `make` in root project directory.

### Run tests or benchmarks ###
All binaries (main executable, unit tests, etc.) will be in `bin/{build_type}`.


## Known Issues ##
 Cachelot is still in 'work in progress' state
 * Some Memcached commands not implemented yet (`cas`, `touch`, `append`, `prepend`, `stats`)
 * Windows build is broken
 * There is no UDP and binary protocol support
 * Cachelot library doesn't have proper build packaging

## Contact ##
 * [www.cachelot.io](http://www.cachelot.io)
 * [Facebook](https://www.facebook.com/cachelot.io)
 * [Twitter](https://twitter.com/cachelot_io)
 * [Bugs](http://dev.cachelot.io/cachelot/issues)
 * [Github mirror](http://github.com/cachelot)


## License ##
Cachelot distributed under the terms of Simplified BSD License<br/>
[http://opensource.org/licenses/BSD-2-Clause](http://opensource.org/licenses/BSD-2-Clause)

## Credits ##
 * [boost C++ libraries](http://www.boost.org)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)
 * Thanks to all open source community
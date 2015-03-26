Cachelot
========

Cachelot is high-performance caching library and distributed caching server. It is compatible with [Memcached](http://memcached.org) but introduces higher performance, less memory consumption, and some new features. 

How To Build
------------

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

Build with IDE or by running `make` in root project directory. All binaries (main executable, unit tests, etc.) will be in `bin/{build_type}`.



Known Issues
------------
*TODO*: contents

Contact
-------
 * [www.cachelot.io](http://www.cachelot.io)
 * [Facebook](https://www.facebook.com/cachelot.io)
 * [Twitter](https://twitter.com/cachelot_io)
 * [Bugs](http://dev.cachelot.io/cachelot/issues)
 * [Github mirror](http://github.com/cachelot)


Credits
-------
 * [boost C++ libraries](http://www.boost.org)
 * [C++ String Toolkit Library](http://www.partow.net/programming/strtk/index.html)

License
-------
*Cachelot* distributed under the terms of Simplified BSD License
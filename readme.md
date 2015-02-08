Cachelot                         {#mainpage}
========

*Cachelot* is memory key-value cache. It is compatible with [memcached](http://memcached.org/), but introduces higher performance, less memory consumption and new features.


## How To Build

### Prerequisites
 * C++11 capable compiler
 * [boost libraries](http://boost.org/) either includes / libraries must be at the standard locations or `${BOOST_ROOT}` environment variable must be set
 * [cmake](http://cmake.org/)
 * optionally [Doxygen](http://doxygen.org/) to build docs

### Build
Clone project repository

    $ hg clone https://d.rider@bitbucket.org/d.rider/cachelot
    $ cd cachelot

Generate project files for your favorite IDE or Makefile by running `cmake -G "{generator}"` in main project directory.
For example:

    $ cmake -G "Unix Makefiles"

or

    $ cmake -G "Xcode"

It's possible to configure build by passing parameters to cmake:

 * `-DFORCE_32_BITS=1` to force creation of 32 bit executable on x64 platform
 * `-DCMAKE_BUILD_TYPE` to choose build type while generationg Makefiles.
   Must be one of:
     - Debug - enable debug messages, assertions and disable optimization
     - Release - release build
     - RelWithDebInfo - release build with debug information enabled
     - MinSizeRel - release build optimized for size
     - AddressSanitizer - special build to run under [Address Sanitizer](https://code.google.com/p/address-sanitizer/) (compiler support required)

Build with IDE or by running `make` in root project directory. All binaries (main executable, unit tests, etc.) will be placed in `./bin/{configuration}` directory.

#### Build using clang and libc++ on Linux (when primary compiler is GCC)
Assume you have working clang and libc++ (including libc++ ABI with headers) at this point
Generate Makefile:

    $ CC=clang CXX=clang++ cmake -G "Unix Makefiles" -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_LIBCXXABI_INCLUDE_PATHS="${path_to}/libcxxabi/include"


Contact
-------
 * [Facebook](https://www.facebook.com/cachelot.io)
 * [Bitbucket](https://bitbucket.org/d.rider/cachelot)

Limitations
-----------
*TODO*: contents

Found a Bug?
------------
*TODO*: contents

How To Contribute
-----------------
*TODO*: contents

Limitations
-----------
*TODO*: contents

Known Issues
------------
*TODO*: contents



#ifndef CACHELOT_H_INCLUDED
#define CACHELOT_H_INCLUDED

/// @defgroup common Generic code
/// @{

#if defined(__GNUC__)
#  pragma GCC system_header  // suppress warnings from the following headers
#endif

// assertion
#ifdef DEBUG
#  include <cassert>
#  define debug_only(expr) expr
#  define debug_assert(expr) assert(expr)
#else
#  define debug_only(expr)
#  define debug_assert(expr)
#endif

// STL
#include <memory>   // unique_ptr
#include <string>   // std::string / std::wstring
#include <vector>   // std::vector
#include <cstdint>  // fixed-size integral types
#include <limits>   // std::numeric_limits
#include <type_traits> // is_base_of, result_of etc.
#include <iostream> // stream IO
#include <algorithm>// min / max
#include <cmath>    // log, ceil
#include <cstdlib>  // strtol, strtoul
#include <stdexcept>// exception / runtime_error, etc
#include <tuple>    // std::tuple
#include <thread>   // std::thread std::this_thread

#define __CACHELOT_PP_STR1(X) #X
#define CACHELOT_PP_STR(X) __CACHELOT_PP_STR1(X)
#define __CACHELOT_PP_CC(a, b) a ## b
#define CACHELOT_PP_CC(a, b) __CACHELOT_PP_CC(a, b)

#if defined(DEBUG) && defined(_MSC_VER)
#  include <crtdbg.h>  // Enable MSVC CRT memory checks
#  define enable_memory_debug() \
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_EVERY_128_DF); \
    _CrtSetReportMode(_CRT_WARN | _CRT_ERROR | _CRT_ASSERT, _CRTDBG_MODE_DEBUG)
#else
#  define enable_memory_debug()
#endif

namespace cachelot {

    /// Fixed size integers
    typedef std::int8_t   int8;
    typedef std::uint8_t  uint8;
    typedef std::int16_t  int16;
    typedef std::uint16_t uint16;
    typedef std::int32_t  int32;
    typedef std::uint32_t uint32;
    typedef std::int64_t  int64;
    typedef std::uint64_t uint64;

    /// Shortcut types
    typedef unsigned int uint;
    typedef unsigned long ulong;

    /// `byte_ptr` is used instead of `void *` where pointer arithmetic is needed
    typedef uint8 * byte_ptr;

    // STD types
    using std::size_t;
    using std::ptrdiff_t;
    using std::nothrow;

    /// string types
    using std::string;
    using std::wstring;

    /// tuple shortcuts
    template <typename ... Args> using tuple = std::tuple<Args...>;
    template <typename ... Args> inline tuple<Args&...> tie(Args&... args) { return std::tie(args...); }
    template <typename ... Args> inline tuple<Args...> make_tuple(Args... args) { return std::make_tuple(args...); }

    /// get raw pointer on type from a smart pointer such as std::unique_ptr
    template <class SmartPointer>
    auto raw_pointer(const SmartPointer & ptr) -> decltype(ptr.get()) {
        return ptr.get();
    }

    constexpr size_t cpu_l1d_cache_line = 64;
    constexpr int the_answer_to_life_the_universe_and_everything = 42;

} // namespace cachelot

/// @}

#endif // CACHELOT_H_INCLUDED


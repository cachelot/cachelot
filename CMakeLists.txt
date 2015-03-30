###########################################################################
#       Cmake basic setup
###########################################################################
if (NOT CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebugInfo;AddressSanitizer" CACHE TYPE INTERNAL FORCE)
    mark_as_advanced (CMAKE_CONFIGURATION_TYPES)
else ()
    # Unix Makefiles support only single build configuration - release by default
    if (NOT "$CMAKE_BUILD_TYPE")
        set (CMAKE_BUILD_TYPE RelWithDebugInfo)
    endif()
endif ()
project (cachelot)
cmake_minimum_required (VERSION 2.8.3)

###########################################################################
#       Platform setup
###########################################################################
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  set (CACHELOT_PLATFORM_BITS 32)
  add_definitions ("-DCACHELOT_PLATFORM_BITS=32")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (CACHELOT_PLATFORM_BITS 64)
  add_definitions ("-DCACHELOT_PLATFORM_BITS=64")
else ()
  message (FATAL_ERROR "CMake reports unsupported pointer size: \"${CMAKE_SIZEOF_VOID_P}\"")
endif ()
message (STATUS "Platform: ${CMAKE_SYSTEM_NAME} x${CACHELOT_PLATFORM_BITS}")
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_definition(-D_WIN32_WINNT=0x05010000) # set NTDDI_WINXP as a target win version
endif ()


###########################################################################
#       Compiler setup
###########################################################################
#  TODO: Precompiled headers macro
message (STATUS "Compiler ID: " ${CMAKE_CXX_COMPILER_ID})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
# - Visual Studio - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # warnings
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    add_definitions ("/wd4510 /wd4511 /wd4512") # default / copy constructor / assignment operator could NOT be generated
    add_definitions ("/wd4996") # function call with parameters that may be unsafe
    add_definitions (-DWIN32_LEAN_AND_MEAN) # limit amount of code in Windows.h to the minimum
    add_definitions (-DNOMINMAX) # disable min / max macro from Windows.h
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc /Zi")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /Ot")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /sdl /MDd /GS /Ob0 /Od /DEBUG /RTC1")
    set (PLATFORM_SPECIFIC_DEBUG_LIBRARIES "Dbghelp.lib")
elseif ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"))
# - GCC / Clang common - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (CACHELOT_PLATFORM_BITS EQUAL 64)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
    elseif (CACHELOT_PLATFORM_BITS EQUAL 32)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    else ()
        message (FATAL_ERROR "CACHELOT_PLATFORM_BITS is set to unknown value: \"${CACHELOT_PLATFORM_BITS}\"")
    endif ()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -fstrict-overflow -fstrict-aliasing -Wshadow -Wstrict-aliasing -Wstrict-overflow=4 -std=c++11")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -DDEBUG")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    set (CMAKE_CXX_FLAGS_RELWITHDEBUGINFO "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG -g")
    set (PLATFORM_SPECIFIC_DEBUG_LIBRARIES "")
    # Flags for address sanitizer build type
    set (ADRESS_SANITIZER_FLAGS "-O1 -g -DNDEBUG -DADDRESS_SANITIZER -fno-optimize-sibling-calls")
    # - - GCC specific - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set (GCC_VERSION ${CMAKE_CXX_COMPILER_VERSION})
        message ("gcc version " ${GCC_VERSION})
        if (GCC_VERSION VERSION_LESS 4.8)
            message (FATAL_ERROR "gcc minimum version 4.8 is required")
        endif ()
        message ("Address sanitizer build type was enabled for gcc")
        set (CMAKE_CXX_FLAGS_ADDRESSSANITIZER "-fsanitize=address ${ADRESS_SANITIZER_FLAGS}")
       # - - Clang specific - - - - - - - - - - - - - - - - - - - - - - - -
    elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        set (CLANG_VERSION ${CMAKE_CXX_COMPILER_VERSION})
        message ("Clang version " ${CLANG_VERSION})
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic")
        set (CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -stdlib=libc++")
        if ((CLANG_VERSION VERSION_EQUAL 3.1) OR (CLANG_VERSION VERSION_GREATER 3.1))
            message ("Address sanitizer build type was enabled for Clang")
            set (CMAKE_CXX_FLAGS_ADDRESSSANITIZER "-faddress-sanitizer ${ADRESS_SANITIZER_FLAGS}")
        endif ()
        # Clang on Linux requires additional flags to use proper ABI
        if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -S -mllvm --x86-asm-syntax=intel")   # Generate ASM code
            set (CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -lc++abi")
        endif ()
    endif ()
else ()
    message (FATAL_ERROR "unknown compiler")
endif ()


###########################################################################
#       3rd party libraries
###########################################################################
# - boost libraries - - - - - - - - - - - - - - - - - - - - - - - - - - - -
set (BOOST_MIN_VERSION, "1.53.0")
set (USED_COMPONENTS unit_test_framework system program_options)
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_LIBS ON)
add_definitions (-DBOOST_ALL_NO_LIB) # disable boost auto-linking feature as it does not work with all compilers
find_package (Boost ${BOOST_MIN_VERSION} COMPONENTS ${USED_COMPONENTS} REQUIRED)
include_directories (${INCLUDE_DIRECTORIES} ${Boost_INCLUDE_DIRS})


###########################################################################
#       Targets
###########################################################################
message (STATUS "Cachelot setup ...")
include_directories (src)
add_subdirectory (src/cachelot)
get_directory_property(CACHELOT_SOURCES DIRECTORY src/cachelot DEFINITION CACHELOT_SOURCES)
list (APPEND CACHELOT_SOURCES src/cachelot/main.cpp) # add main.cpp containig main() function
add_executable (cachelot ${CACHELOT_SOURCES})
set (CACHELOT_LINK_LIBRARIES ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})
# - target link libraries setup
target_link_libraries (cachelot ${CACHELOT_LINK_LIBRARIES})
list (LENGTH PLATFORM_SPECIFIC_DEBUG_LIBRARIES NUM_PLATFORM_SPECIFIC_DEBUG_LIBRARIES)
if (${NUM_PLATFORM_SPECIFIC_DEBUG_LIBRARIES})
  target_link_libraries (cachelot debug ${PLATFORM_SPECIFIC_DEBUG_LIBRARIES})
  message(STATUS "Platform specific debug libraries: " ${PLATFORM_SPECIFIC_DEBUG_LIBRARIES})
endif ()

set (CACHELOT_BUILD "")
string (TOLOWER "${CMAKE_BUILD_TYPE}" CACHELOT_BUILD)
set_target_properties (cachelot PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin/${CACHELOT_BUILD})

# - - - Unit tests - - - - - - - - - -
add_subdirectory (src/unit_test)
# - - - Benchmarks - - - - - - - - - -
add_subdirectory (src/benchmark)


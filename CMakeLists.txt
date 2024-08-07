###########################################################################
#       Cmake basic setup
###########################################################################
if (NOT CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebugInfo;MinSizeRel;AddressSanitizer;UBSanitizer" CACHE STRING INTERNAL FORCE)
    mark_as_advanced (CMAKE_CONFIGURATION_TYPES)
else ()
    # Unix Makefiles support only single build configuration - release by default
    if (NOT CMAKE_BUILD_TYPE)
        set (CMAKE_BUILD_TYPE RelWithDebugInfo CACHE STRING INTERNAL FORCE)
    endif ()
endif ()
project (cachelot)
cmake_minimum_required (VERSION 2.8.3)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_STANDARD 11)

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
    add_definitions (-D_WIN32_WINNT=0x06010000) # set NTDDI_WIN7 as a target win version
endif ()
string (REGEX MATCH "arm.*" ARM_CPU "${CMAKE_SYSTEM_PROCESSOR}")
string (REGEX MATCH "x86_64" x86_64_CPU "${CMAKE_SYSTEM_PROCESSOR}")

###########################################################################
#       Compiler setup
###########################################################################
#  TODO: Precompiled headers macro
message (STATUS "Compiler: " ${CMAKE_CXX_COMPILER_ID} " v" ${CMAKE_CXX_COMPILER_VERSION})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
# - Visual Studio - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # warnings
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    add_definitions ("/wd4510 /wd4511 /wd4512") # default / copy constructor / assignment operator could NOT be generated
    add_definitions ("/wd4996") # function call with parameters that may be unsafe
    add_definitions (-DWIN32_LEAN_AND_MEAN) # limit amount of code in Windows.h to the minimum
    add_definitions (-DNOMINMAX) # disable min / max macro from Windows.h
    set (CMAKE_C_FLAGS "/W4 /EHsc")
    set (CMAKE_C_FLAGS_RELEASE "/MD /Ot")
    set (CMAKE_C_FLAGS_DEBUG "/ZI /sdl /MDd /GS /Ob0 /Od /DEBUG /RTC1")
    set (DEBUG_LIBRARIES "Dbghelp.lib")
    add_compile_options (/permissive-)
elseif ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$"))
# - GCC / Clang common - - - - - - - - - - - - - - - - - - - - - - - - - -
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -fstrict-overflow -fstrict-aliasing -Wshadow -Wstrict-aliasing -Wstrict-overflow=4")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64_CPU)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -msse4")
        if (CACHELOT_PLATFORM_BITS EQUAL 64)
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -m64")
        endif ()
    endif ()
    set (CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")
    set (CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    set (CMAKE_C_FLAGS_RELWITHDEBUGINFO "-O2 -DNDEBUG -g")
    set (CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")
    set (CMAKE_C_FLAGS_ADDRESSSANITIZER "-fsanitize=address -O2 -g -DNDEBUG -DADDRESS_SANITIZER -fno-optimize-sibling-calls")
    set (CMAKE_C_FLAGS_UBSANITIZER "-fsanitize=undefined -O2 -g -DNDEBUG -DUB_SANITIZER -fno-optimize-sibling-calls")

    set (DEBUG_LIBRARIES "")
    # - - Clang specific - - - - - - - - - - - - - - - - - - - - - - - -
    if (CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic")
        # set libc++ as a default std library (if available)
        if (NOT ${CMAKE_CXX_FLAGS} MATCHES "-stdlib=")
            include(CheckLibraryExists)
            check_library_exists(c++ throw "" HAVE_LIBCPLUSPLUS)
            if (HAVE_LIBCPLUSPLUS)
                set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
                set (CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++ -lc++abi")
            endif ()
        endif ()
    endif ()
else ()
    message (FATAL_ERROR "unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()
set (CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
set (CMAKE_CXX_FLAGS_RELWITHDEBUGINFO ${CMAKE_C_FLAGS_RELWITHDEBUGINFO})
set (CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
set (CMAKE_CXX_FLAGS_ADDRESSSANITIZER ${CMAKE_C_FLAGS_ADDRESSSANITIZER})
set (CMAKE_CXX_FLAGS_UBSANITIZER ${CMAKE_C_FLAGS_UBSANITIZER})

###########################################################################
#       CCache configuration
###########################################################################
find_program (CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    message (STATUS "Enable ccache")
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif (CCACHE_FOUND)

###########################################################################
#       config.h
###########################################################################
include (CheckCXXSymbolExists)
check_cxx_symbol_exists (aligned_alloc stdlib.h HAVE_ALIGNED_ALLOC)
check_cxx_symbol_exists (posix_memalign stdlib.h HAVE_POSIX_MEMALIGN)
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/src/cachelot/config.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/cachelot/config.h")
add_definitions (-DHAVE_CONFIG_H=1)

###########################################################################
#       version.h
###########################################################################
set (version_valid 0)
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --dirty=-git OUTPUT_VARIABLE cachelot_git_tag WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (git_version_regex "v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([A-z]+)(-[0-9]+-)?(.*)$")
    string (REGEX MATCH "${git_version_regex}" version_valid "${cachelot_git_tag}")
endif ()
if (version_valid)
    string (REGEX REPLACE "${git_version_regex}" "\\1;\\2;\\3;\\4;\\6" parsed_version_list "${cachelot_git_tag}")
    list (GET parsed_version_list 0 cachelot_version_major)
    list (GET parsed_version_list 1 cachelot_version_minor)
    list (GET parsed_version_list 2 cachelot_version_patch)
    list (GET parsed_version_list 3 cachelot_version_extra)
    list (GET parsed_version_list 4 cachelot_version_hash)
else (version_valid)
    message (AUTHOR_WARNING "Bad version ${cachelot_git_tag};")
    set (cachelot_version_major "?")
    set (cachelot_version_minor "?")
    set (cachelot_version_patch "?")
    set (cachelot_version_extra "unknown")
    set (cachelot_version_hash "XXX")
endif ()
set (cachelot_os_name "${CMAKE_SYSTEM_NAME} x${CACHELOT_PLATFORM_BITS}")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/src/cachelot/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/cachelot/version.h" ESCAPE_QUOTES @ONLY)

###########################################################################
#       3rd party libraries
###########################################################################
# - boost libraries - - - - - - - - - - - - - - - - - - - - - - - - - - - -
set (BOOST_MIN_VERSION, "1.53.0")
set (USED_COMPONENTS system program_options)
if (BUILD_TESTS)
    set(USED_COMPONENTS ${USED_COMPONENTS} unit_test_framework)
endif()
set (Boost_USE_MULTITHREADED ON CACHE BOOL "Use multithreaded Boost")
set (Boost_USE_STATIC_LIBS ON CACHE BOOL "Use static Boost libraries")
add_definitions (-DBOOST_ALL_NO_LIB) # disable boost auto-linking feature as it does not work with all compilers
find_package (Boost ${BOOST_MIN_VERSION} COMPONENTS ${USED_COMPONENTS} REQUIRED)
include_directories (${INCLUDE_DIRECTORIES} ${Boost_INCLUDE_DIRS})


###########################################################################
#       Targets
###########################################################################
message (STATUS "Cachelot setup ...")
set (NO_SERVER OFF CACHE BOOL "Do not build the server")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}")
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
# - Visual Studio - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # Deactivate intermediate directory
    set (CMAKE_INTDIR "")
    set (CMAKE_CFG_INTDIR "")
    # Setup output directory specifically for current configuration
    string (TOUPPER ${CMAKE_BUILD_TYPE} UPPERBUILD )
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_${UPPERBUILD} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_${UPPERBUILD} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${UPPERBUILD} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()
include_directories (src)
# - - - cachelot library - - - - - - -
add_subdirectory (src/cachelot)
if (NOT NO_SERVER)
# - - - cachelotd server - - - - - - -
add_subdirectory (src/server)
endif()
if (BUILD_TESTS)
# - - - unit tests - - - - - - - - - -
add_subdirectory (src/unit_test)
# - - - C API test - - - - - - - - - -
add_subdirectory (src/test_c_api)
endif()
# - - - Benchmarks - - - - - - - - - -
add_subdirectory (src/benchmark)


###########################################################################
#       Install
###########################################################################

set (INSTALL_CMAKEDIR share/${PROJECT_NAME})
install (TARGETS cachelot EXPORT cachelotTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
install (EXPORT cachelotTargets DESTINATION ${INSTALL_CMAKEDIR})

include (CMakePackageConfigHelpers)

configure_package_config_file (${PROJECT_SOURCE_DIR}/cmake/cachelot-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cachelot-config.cmake INSTALL_DESTINATION ${INSTALL_CMAKEDIR})
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/cachelot-config.cmake DESTINATION ${INSTALL_CMAKEDIR})

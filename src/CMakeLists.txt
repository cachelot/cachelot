if (NOT CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set (CMAKE_CONFIGURATION_TYPES debug release asan CACHE TYPE INTERNAL FORCE)
else ()
   # Unix Makefiles support only single build configuration - release by default
   set (CMAKE_BUILD_TYPE release)
endif ()
project (cachelot)
cmake_minimum_required (VERSION 2.8.3)
# Convert build type to lower case for case insensitive comparisson
string (TOLOWER "${CMAKE_BUILD_TYPE}" CACHELOT_BUILD)

# 32-bit OS forces to build x32 binaries
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  set (FORCE_32_BITS TRUE)
else ()
  set (FORCE_32_BITS FALSE)
endif ()

if (CACHELOT_BUILD STREQUAL "asan")
  message ("Yo ho ho!")
else ()
  message ("Putin huilo")
endif ()



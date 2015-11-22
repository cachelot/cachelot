#ifndef CACHELOT_VERSION_H_INCLUDED
#define CACHELOT_VERSION_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#define CACHELOT_VERSION_MAJOR 0
#define CACHELOT_VERSION_MINOR 8
#define CACHELOT_VERSION_STATUS "alpha"
// TODO: cmake
#define CACHELOT_GIT_REVSION "@cachelot_git_revision@"
#define CACHELOT_COMPILER_INFO "@cachelot_compiler_info@"
#define CACHELOT_COMPILER_FLAGS "@cachelot_compiler_flags@"

#if defined(DEBUG)
#  define CACHELOT_VERSION_DBG_SUFFIX "dbg"
#else
#  define CACHELOT_VERSION_DBG_SUFFIX
#endif

#define CACHELOT_VERSION_STRING "Cachelot " CACHELOT_PP_STR(CACHELOT_VERSION_MAJOR) "." CACHELOT_PP_STR(CACHELOT_VERSION_MINOR) CACHELOT_VERSION_DBG_SUFFIX " (" CACHELOT_VERSION_STATUS ")"


#endif // CACHELOT_VERSION_H_INCLUDED

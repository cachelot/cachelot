#ifndef CACHELOT_UNIT_TEST_H_INCLUDED
#define CACHELOT_UNIT_TEST_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <cachelot/common.h>
#include <cachelot/random.h>

#if defined(__GNUC__)
// suppress warnings from the following headers
#  pragma GCC system_header
#endif

// disable auto linking (#pragma lib)
#define BOOST_TEST_NO_LIB
// Enable messages from unit tests
#define BOOST_TEST_LOG_LEVEL boost::unit_test::log_messages
#include <boost/test/unit_test.hpp>
#include <boost/test/debug.hpp>

namespace cachelot {


} // namespace cachelot

#endif // CACHELOT_UNIT_TEST_H_INCLUDED

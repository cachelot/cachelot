#include "unit_test.h"
#include <cachelot/atom.h>

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_atom)

BOOST_AUTO_TEST_CASE(test_empty)
{
    BOOST_CHECK_EQUAL(atom("1"), atom("1"));
    BOOST_CHECK_EQUAL(atom("atom_atom1"), atom("atom_atom1"));
    BOOST_CHECK_NE(atom("atom_atom1"), atom("atom_atom2"));
    BOOST_CHECK_NE(atom("1atom_atom"), atom("2atom_atom"));
}


BOOST_AUTO_TEST_SUITE_END()


#include "unit_test.h"
#include <cachelot/intrusive_list.h>


namespace {

using namespace cachelot;


struct test_list_item {
    int dummy;
    intrusive_list_node link;
};

BOOST_AUTO_TEST_SUITE(test_intrusive_list)

BOOST_AUTO_TEST_CASE(test_list_ops) {

    test_list_item b1, b2, b3;

    typedef intrusive_list<test_list_item, &test_list_item::link> test_list_type;

    test_list_type the_list;
    BOOST_CHECK(the_list.empty());
    // single item basic operations
    the_list.push_front(&b1);
    BOOST_CHECK(! the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b1);
    BOOST_CHECK_EQUAL(the_list.back(), &b1);
    BOOST_CHECK(the_list.is_head(&b1));
    BOOST_CHECK(the_list.is_tail(&b1));
    BOOST_CHECK_EQUAL(the_list.pop_back(), &b1);
    BOOST_CHECK(the_list.empty());
    the_list.push_back(&b1);
    BOOST_CHECK_EQUAL(the_list.back(), &b1);
    BOOST_CHECK_EQUAL(the_list.front(), &b1);
    BOOST_CHECK(the_list.is_head(&b1));
    BOOST_CHECK(the_list.is_tail(&b1));
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b1);
    BOOST_CHECK(the_list.empty());

    // multiple item operations
    the_list.push_front(&b1);
    the_list.push_front(&b2);
    the_list.push_back(&b3);
    BOOST_CHECK(! the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove one item
    test_list_type::unlink(&b1);
    BOOST_CHECK(! the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b2));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove second element
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b2);
    BOOST_CHECK(! the_list.empty());
    BOOST_CHECK_EQUAL(the_list.front(), &b3);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    BOOST_CHECK(the_list.is_head(&b3));
    BOOST_CHECK(the_list.is_tail(&b3));
    // remove last element
    test_list_type::unlink(&b3);
    BOOST_CHECK(the_list.empty());
    // movement
    the_list.push_front(&b1);
    the_list.push_front(&b2);
    the_list.push_front(&b3);
    BOOST_CHECK_EQUAL(the_list.front(), &b3);
    the_list.move_front(&b3);
    BOOST_CHECK_EQUAL(the_list.front(), &b3);
    the_list.move_front(&b2);
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    the_list.move_front(&b1);
    BOOST_CHECK_EQUAL(the_list.front(), &b2);
    BOOST_CHECK_EQUAL(the_list.back(), &b3);
    the_list.move_front(&b1);
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b1);
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b2);
    BOOST_CHECK_EQUAL(the_list.pop_front(), &b3);
    BOOST_CHECK(the_list.empty());
}


BOOST_AUTO_TEST_SUITE_END()


} // anonymouse namespace


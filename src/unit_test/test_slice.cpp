#include "unit_test.h"
#include <cachelot/slice.h>

namespace {

using namespace cachelot;

static const char HelloStr1[] = "Hello, World!";
static const char HelloStr2[] = "Hello, World?";
static const size_t HelloStrLength = strlen(HelloStr1);

BOOST_AUTO_TEST_SUITE(test_bytes)

BOOST_AUTO_TEST_CASE(test_basic) {
    // constructors / empty / length / operator bool / etc
    slice hello1;
    BOOST_CHECK(hello1.empty() && ! hello1);
    BOOST_CHECK(hello1.length() == 0);
    hello1 = slice::from_literal(HelloStr1);
    slice hello2 = hello1;
    BOOST_CHECK(hello2 && hello2.length() == HelloStrLength);
    BOOST_CHECK(hello1 == hello2);
    hello2 = slice::from_literal(HelloStr2);
    BOOST_CHECK(hello1 != hello2);
    BOOST_CHECK(hello1[5] == *hello1.nth(5));
    BOOST_CHECK(hello1[5] == hello2[5] && hello1[5] == ',');
    // split / equals / search / startswith / endswith
    slice first_word1, rest1;
    tie(first_word1, rest1) = hello1.split(slice::from_literal(", "));
    BOOST_CHECK(first_word1 == slice::from_literal("Hello"));
    BOOST_CHECK(rest1 == slice::from_literal("World!"));
    slice lookup = hello1.search(slice::from_literal("World"));
    BOOST_CHECK(lookup && 'W' == lookup[0] && lookup.length() == strlen("World"));
    slice first_word2, rest2;
    tie(first_word2, rest2) = hello2.split(slice::from_literal(", "));
    BOOST_CHECK(first_word1 == first_word2);
    first_word1 = hello1.subslice(0, 5);
    BOOST_CHECK(first_word1 == first_word2);
    BOOST_CHECK(hello1.subslice(0, hello1.length()-1) == hello2.subslice(0, hello2.length()-1));
    BOOST_CHECK(hello2.contains(hello1.subslice(1, hello1.length()-2)));
    tie(first_word1, rest1) = hello1.split(slice::from_literal("?!?!?!?"));
    BOOST_CHECK(first_word1 == hello1 && rest1.empty());
    lookup = hello1.search(slice::from_literal(", "));
    tie(first_word1, rest1) = hello1.split_at(lookup.begin());
    tie(first_word2, rest2) = hello2.split(slice::from_literal(", "));
    BOOST_CHECK(first_word1 == first_word2);
    BOOST_CHECK(rest1.startswith(slice::from_literal(", ")));
    BOOST_CHECK(rest2.endswith(slice::from_literal("World?")));
}


BOOST_AUTO_TEST_SUITE_END()

} //anonymous namespace


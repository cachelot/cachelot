#include "unit_test.h"
#include <cachelot/bytes.h>

namespace {

using namespace cachelot;

static const char HelloStr1[] = "Hello, World!";
static const char HelloStr2[] = "Hello, World?";
static const size_t HelloStrLength = strlen(HelloStr1);

BOOST_AUTO_TEST_SUITE(test_bytes)

BOOST_AUTO_TEST_CASE(test_basic) {
    // constructors / empty / length / operator bool / etc
    bytes hello1;
    BOOST_CHECK(hello1.empty() and not hello1);
    BOOST_CHECK(hello1.length() == 0);
    hello1 = bytes::from_literal(HelloStr1);
    bytes hello2 = hello1;
    BOOST_CHECK(hello2 and hello2.length() == HelloStrLength);
    BOOST_CHECK(hello1.equals(hello2));
    hello2 = bytes::from_literal(HelloStr2);
    BOOST_CHECK(not hello1.equals(hello2));
    BOOST_CHECK(hello1[5] == *hello1.nth(5));
    BOOST_CHECK(hello1[5] == hello2[5] and hello1[5] == ',');
    // split / equals / search / startswith / endswith
    bytes first_word1, rest1;
    tie(first_word1, rest1) = hello1.split(bytes::from_literal(", "));
    BOOST_CHECK(first_word1.equals(bytes::from_literal("Hello")));
    BOOST_CHECK(rest1.equals(bytes::from_literal("World!")));
    bytes lookup = hello1.search(bytes::from_literal("World"));
    BOOST_CHECK(lookup and 'W' == lookup[0] and lookup.length() == strlen("World"));
    bytes first_word2, rest2;
    tie(first_word2, rest2) = hello2.split(bytes::from_literal(", "));
    BOOST_CHECK(first_word1.equals(first_word2));
    first_word1 = hello1.slice(0, 5);
    BOOST_CHECK(first_word1.equals(first_word2));
    BOOST_CHECK(hello1.slice(0, hello1.length()-1).equals(hello2.slice(0, hello2.length()-1)));
    BOOST_CHECK(hello2.contains(hello1.slice(1, hello1.length()-2)));
    tie(first_word1, rest1) = hello1.split(bytes::from_literal("?!?!?!?"));
    BOOST_CHECK(first_word1.equals(hello1) and rest1.empty());
    lookup = hello1.search(bytes::from_literal(", "));
    tie(first_word1, rest1) = hello1.split_at(lookup.begin());
    tie(first_word2, rest2) = hello2.split(bytes::from_literal(", "));
    BOOST_CHECK(first_word1.equals(first_word2));
    BOOST_CHECK(rest1.startswith(bytes::from_literal(", ")));
    BOOST_CHECK(rest2.endswith(bytes::from_literal("World?")));
}


BOOST_AUTO_TEST_SUITE_END()

} //anonymous namespace


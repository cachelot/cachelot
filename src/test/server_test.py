#!/usr/bin/env python
#
# Cachelot server test suite
#

import sys
import random
import string
import memcached


class TestFailedError(Exception):
    pass


def CHECK(condition):
    if not condition:
        raise TestFailedError()


def random_string(minlen, maxlen):
    assert minlen > 1, 'minlen is too small'
    assert minlen <= maxlen, 'invalid lengths range'
    length = random.randint(minlen, maxlen)
    return ''.join(random.choice(string.ascii_letters + string.digits) for _ in range(length))


def smoke_test(mc):
    "Test basic functionality"
    key1 = random_string(10, 50)
    value1 = random_string(100, 400)
    # replace, add, get, delete
    CHECK( mc.replace(key1, value1) == False )
    CHECK( mc.add(key1, value1) == True )
    CHECK( mc.add(key1, value1) == False )  # second add shall fail
    CHECK( mc.get(key1) == value1 )
    value1 = random_string(100, 400)
    CHECK( mc.replace(key1, value1) == True )
    CHECK( mc.get(key1) == value1 )
    CHECK( mc.delete(key1) == True )
    # set, cas
    key2 = random_string(10, 50)
    value2 = random_string(100, 400)
    mc.set(key2, value2)
    CHECK( mc.get(key2) == value2 )
    k2_ret_value, k2_cas = mc.gets(key2)
    CHECK( k2_ret_value == value2 )
    value2 = random_string(100, 400)
    CHECK( mc.cas(key2, value2, 0, k2_cas) == True )
    CHECK( mc.cas(key2, value2, 0, k2_cas) == False )  # second cas shall fail
    CHECK( mc.get(key2) == value2 )
    # cas with replace
    k2_ret_value, k2_cas = mc.gets(key2)
    value2 = random_string(100, 400)
    mc.replace(key2, value2)
    CHECK( mc.cas(key2, value2, 0, k2_cas) == False )  # cas after replace shall fail
    CHECK( mc.get(key2) == value2 )
    # cas with set
    k2_ret_value, k2_cas = mc.gets(key2)
    value2 = random_string(100, 400)
    mc.set(key2, value2)
    CHECK( mc.cas(key2, value2, 0, k2_cas) == False )  # cas after set shall fail
    CHECK( mc.get(key2) == value2 )
    # multikey get, gets


def fuzzy_test(mc):
    for num_items in range(10000):
        pass


def main():
    mc = memcached.connect_tcp('localhost', 11211)
    smoke_test(mc)


if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)
    main()


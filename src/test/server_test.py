#!/usr/bin/env python
#
# Cachelot server test suite
#

import sys
import logging
import random
import string
import memcached

log = logging.Logger('Cachelot Test')


class TestFailedError(Exception):
    pass


def CHECK(condition):
    if not condition:
        raise TestFailedError()


def CHECK_EQ(v1, v2):
    if v1 != v2:
        raise TestFailedError(str(v1) + ' != ' + str(v2))


def CHECK_EXC(fcn, exc):
    try:
        fcn()
        raise TestFailedError('Exception ' + str(exc) + ' was not raised as expected')
    except exc as e:
        return


KEY_ALPHABET = string.ascii_letters + string.digits


def random_key():
    length = random.randint(10, 250)
    return ''.join(random.choice(KEY_ALPHABET) for _ in range(length))
    

def random_value():
    length = random.randint(1, 524288)
    return ''.join(chr(random.randint(0, 254)) for _ in range(length))


def basic_storage_test(mc):
    "Test set/get/add/delete/replace commands"
    key1 = random_key()
    value1 = random_value()
    # replace, add, get, delete
    CHECK_EQ( mc.replace(key1, value1), False )
    CHECK_EQ( mc.add(key1, value1), True )
    CHECK_EQ( mc.add(key1, value1), False )  # second add shall fail
    CHECK_EQ( mc.get(key1), value1 )
    value1 = random_value()
    CHECK_EQ( mc.replace(key1, value1), True )
    CHECK_EQ( mc.get(key1), value1 )
    CHECK_EQ( mc.delete(key1), True )
   

def basic_cas_test(mc):
    "Test cas (compare and swap) command"
    # simple cas
    k = random_key()
    v = random_value()
    mc.set(k, v)
    CHECK_EQ( mc.get(k), v )
    retval, ver = mc.gets(k)
    CHECK_EQ( retval, v )
    v = random_value()
    CHECK_EQ( mc.cas(k, v, 0, ver), True )
    CHECK_EQ( mc.cas(k, v, 0, ver), False )  # second cas shall fail
    CHECK_EQ( mc.get(k), v )
    # cas after replace
    __, ver = mc.gets(k)
    v = random_value()
    mc.replace(k, v)
    CHECK_EQ( mc.cas(k, v, 0, ver), False )  # cas after replace shall fail
    CHECK_EQ( mc.get(k), v )
    # cas after set
    __, ver = mc.gets(k)
    v = random_value()
    mc.set(k, v)
    CHECK_EQ( mc.cas(k, v, 0, ver), False )  # cas after set shall fail
    CHECK_EQ( mc.get(k), v )


def basic_batch_op_test(mc):
    "Test multi-get and batch set operations"
    test_dict = {}
    for i in range(20):
        k = random_key()
        v = random_value()
        mc.set(k, v)
        test_dict[k] = v
    for k, v in mc.get_multi(test_dict.keys()):
        CHECK_EQ( test_dict[k], v )


def basic_expiration_test(mc):
    "Test object expiration and touch command"
    pass


def basic_arithmetic_test(mc):
    "Test incr/decr commands"
    key1 = "key1"
    mc.set(key1, "00000000000000")
    CHECK_EQ( mc.incr(key1, 1), 1 )
    CHECK_EQ( mc.decr(key1, 1), 0 )
    CHECK_EQ( mc.incr(key1, 0), 0 )
    CHECK_EQ( mc.decr(key1, 1), 0 )
    CHECK_EQ( mc.incr(key1, 4294967296), 4294967296 )
    CHECK_EQ( mc.decr(key1, 4294967295), 1 )
    CHECK_EQ( mc.decr(key1, 4294967295), 0 )
    CHECK_EXC( lambda: mc.incr("non-existing-key", 1), memcached.KeyNotFoundError )
    CHECK_EXC( lambda: mc.decr("non-existing-key", 1), memcached.KeyNotFoundError )


def run_smoke_test(mc):
    "Test basic functionality"
    for run_no in range(100):
        #basic_storage_test(mc)
        #basic_cas_test(mc)
        #basic_batch_op_test(mc)
        #basic_expiration_test(mc)
        basic_arithmetic_test(mc)


def run_fuzzy_test(mc):
    for num_items in range(10000):
        pass


def main():
    mc = memcached.connect_tcp('localhost', 11211)
    run_smoke_test(mc)


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    main()


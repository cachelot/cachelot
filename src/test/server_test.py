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
    CHECK( mc.replace(key1, value1) == False )
    CHECK( mc.add(key1, value1) == True )
    CHECK( mc.add(key1, value1) == False )  # second add shall fail
    CHECK( mc.get(key1) == value1 )
    value1 = random_value()
    CHECK( mc.replace(key1, value1) == True )
    CHECK( mc.get(key1) == value1 )
    CHECK( mc.delete(key1) == True )
   

def basic_cas_test(mc):
    "Test cas (compare and swap) command"
    # simple cas
    k = random_key()
    v = random_value()
    mc.set(k, v)
    CHECK( mc.get(k) == v )
    retval, ver = mc.gets(k)
    CHECK( retval == v )
    v = random_value()
    CHECK( mc.cas(k, v, 0, ver) == True )
    CHECK( mc.cas(k, v, 0, ver) == False )  # second cas shall fail
    CHECK( mc.get(k) == v )
    # cas after replace
    __, ver = mc.gets(k)
    v = random_value()
    mc.replace(k, v)
    CHECK( mc.cas(k, v, 0, ver) == False )  # cas after replace shall fail
    CHECK( mc.get(k) == v )
    # cas after set
    __, ver = mc.gets(k)
    v = random_value()
    mc.set(k, v)
    CHECK( mc.cas(k, v, 0, ver) == False )  # cas after set shall fail
    CHECK( mc.get(k) == v )


def basic_batch_op_test(mc):
    "Test multi-get and batch set operations"
    test_dict = {}
    for i in range(100):
        k = random_key()
        v = random_value()
        mc.set(k, v)
        test_dict[k] = v
    for k, v in mc.get_multi(test_dict.keys()):
        CHECK( test_dict[k] == v )


def basic_expiration_test(mc):
    "Test object expiration and touch command"
    pass


def basic_arithmetic_test(mc):
    "Test incr/decr commands"
    pass


def run_smoke_test(mc):
    "Test basic functionality"
    for run_no in range(100):
        basic_storage_test(mc)
        basic_cas_test(mc)
        basic_batch_op_test(mc)
        basic_expiration_test(mc)
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


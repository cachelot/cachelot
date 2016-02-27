#!/usr/bin/env python
#
# Cachelot server test suite
#

import sys
import logging
import random
import string
import memcached
import time
import os
import subprocess


SELF, _ = os.path.splitext(os.path.basename(sys.argv[0]))
log = logging.getLogger(SELF)


class TestFailedError(Exception):
    pass


def CHECK(condition):
    if not condition:
        raise TestFailedError()


def CHECK_EQ(v1, v2):
    if v1 != v2:
        v1 = str(v1)[:30]
        v2 = str(v2)[:30]
        raise TestFailedError(v1 + ' != ' + v2)


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
    length = random.randint(1, 100000)
    return ''.join(chr(random.randint(0, 254)) for _ in range(length))


def basic_storage_test(mc):
    log.info("set/get/add/delete/replace/append/prepend commands")
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
    CHECK_EXC( lambda: mc.delete(key1), memcached.KeyNotFoundError )
    # append / prepend
    k = random_key()
    v = random_value()
    mc.set(k, v)
    v1 = random_value()
    mc.append(k, v1)
    v += v1
    CHECK_EQ( mc.get(k), v )
    v1 = random_value()
    mc.prepend(k, v1)
    v = v1 + v
    CHECK_EQ( mc.get(k), v )
    log.info("-   success")


def basic_cas_test(mc):
    log.info("cas (compare and swap) command")
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

    log.info("-   success")


def basic_batch_op_test(mc):
    log.info("multi-get and batch set operations")
    test_dict = {}
    for i in range(20):
        k = random_key()
        v = random_value()
        mc.set(k, v)
        test_dict[k] = v
    for k, v in mc.get_multi(test_dict.keys()):
        CHECK_EQ( test_dict[k], v )
    log.info("-   success")


def basic_expiration_test(mc):
    log.info("object expiration and touch command")
    k = random_key()
    v = random_value()
    mc.set(k, v, 1)
    time.sleep(1)
    mc.flush_all()
    CHECK_EQ( mc.get(k), None )
    mc.set(k, v, 999)
    time.sleep(1)
    CHECK_EQ( mc.touch(k, 2), True )
    time.sleep(1)
    CHECK_EQ( mc.get(k), v)
    time.sleep(1)
    CHECK_EQ( mc.get(k), None)
    mc.flush_all()
    log.info("-   success")


def basic_arithmetic_test(mc):
    log.info("incr/decr commands")
    key1 = random_key()
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
    log.info("-   success")


def run_smoke_test(mc):
    log.info("Test basic functionality")
    basic_storage_test(mc)
    basic_cas_test(mc)
    basic_batch_op_test(mc)
    basic_expiration_test(mc)
    basic_arithmetic_test(mc)
    log.info("all basic functionality tests passed")


def run_fuzzy_test(mc):
    # TODO: !!!
    pass


def main():
    log.info("Running Cachelot tests ...")
    mc = memcached.connect_tcp('localhost', 11211)
    ver = mc.version()
    log.info("Version: '%s'", ver)
    run_smoke_test(mc)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    main()


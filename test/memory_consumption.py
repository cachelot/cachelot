#!/usr/bin/env python
#
# Cachelot vs Memcached server memory consumption test
#

import sys
import string
import random
import os
import subprocess
import shlex
import time
import logging
import atexit
import memcached


log = logging.getLogger()
toMb = lambda b: b*1.0/1024/1024

SELF, _ = os.path.splitext(os.path.basename(sys.argv[0]))
BASEDIR = os.path.normpath(os.path.dirname(os.path.abspath(sys.argv[0])) + '/..')
MEMORY_LIMIT = 1024*1024*1024*4  # Gb
CACHELOTD = os.path.join(BASEDIR, 'bin/RelWithDebugInfo/cachelotd -m%d' % toMb(MEMORY_LIMIT))
MEMCACHED = '/usr/bin/memcached -m%d' % toMb(MEMORY_LIMIT)
NUM_RUNS = 10

KEY_ALPHABET = string.ascii_letters + string.digits
KEY_MINLEN = 10
KEY_MAXLEN = 60

VALUE_RANGES = [('small',    10,    1024),
                ('medium', 1024,    4096),
                ('large',  4096, 1000000),
                ('all',      10, 1000000)]

def setup_logging():
    """
    Setup python logging module (destinations like terminal output and file, formatting, etc.)
    """
    # logging setup
    logging_default_level = logging.DEBUG
    logging_console_enable = True
    logging_format = '%(levelname)s: %(message)s'
    logging_file_enable = True
    logging_file_dir = './'
    logging_file_name = SELF + time.strftime('_%Y%b%d-%H%M%S') + '.log'
    # Custom STAT level
    logging.STAT = logging.DEBUG + 1
    logging.addLevelName(logging.STAT,  'STAT')

    root_logger = logging.getLogger()
    formatter = logging.Formatter(logging_format)
    # Console output
    if logging_console_enable:
        console = logging.StreamHandler()
        console.setFormatter(formatter)
        root_logger.addHandler(console)
    # Log file
    if logging_file_enable:
        filename = os.path.join(logging_file_dir, logging_file_name)
        logfile = logging.FileHandler(filename)
        logfile.setFormatter(formatter)
        root_logger.addHandler(logfile)
    atexit.register(logging.shutdown)
    root_logger.setLevel(logging_default_level)


def random_key():
    "Generate random key from pre-defined alphabet "
    length = random.randint(KEY_MINLEN, KEY_MAXLEN)
    return ''.join(random.choice(KEY_ALPHABET) for _ in range(length))


def random_value(length):
    "Generate value payload (we care only about length, not contents)"
    return 'X' * length


def log_stats(mc):
    "Print memory related stats of cachelot / memcached to the log"
    FILTER_STATS = frozenset([
        'cmd_get', 'get_hits', 'get_misses',
        'used_memory',
        'total_requested', 'total_served',
        'evictions',
        'curr_items', 'total_items',
        'evicted_unfetched',
    ])
    for stat, value in mc.stats():
        if stat in FILTER_STATS:
            log.log(logging.STAT, '%30s:  %s' % (stat, value))


def shell_exec(command):
    devnull = open(os.devnull, 'w')
    return subprocess.Popen(args=shlex.split(command), stdout=devnull, stderr=devnull)


def create_kv_data(minval, maxval, memlimit):
    """ Generate array of (key, value_length) pairs
        Actual values will be generated randomly every time
        Total size of all keys and values will be around memlimit
    """
    data = []
    data_size = 0
    while data_size < memlimit:
        k = random_key()
        data_size += len(k)
        v_length = random.randint(minval, maxval)
        data_size += v_length
        data.append((k, v_length))

    return data


def execute_test(mc, minval, maxval, memlimit):
    """ Main test function.
        It takes connected memcached client and fills it with random data.
        After that there is an attempt to retrieve data back.
    """
    all_keys = []
    local_effective_mem = 0
    log.info('Fill-in caching server with random data')
    start_time = time.time()
    while local_effective_mem < memlimit:
        key = random_key()
        local_effective_mem += len(key)
        value = random_value(random.randint(minval, maxval))
        local_effective_mem += len(value)
        mc.set(key, value)
        all_keys.append(key)
    log.debug('  Took: %.2f sec', time.time() - start_time)

    log.debug('Checking caching server ...')
    cache_effective_mem = 0
    stored_items = 0
    start_time = time.time()
    for k in all_keys:
        external_val = mc.get(k)
        if external_val:
            cache_effective_mem += len(k) + len(external_val)
            stored_items += 1
    log.debug('  Took: %.2f sec', time.time() - start_time)
    # print statistics
    log_stats(mc)
    log.info('External effective memory: %.02f/%.02f Mb. (%d/%d items)', toMb(cache_effective_mem), stored_items,
                                                                         toMb(local_effective_mem), len(all_keys))
    return (cache_effective_mem, stored_items)


def execute_test_n_times(times, mc, minval, maxval, memlimit):
    all_effective_mem = []
    all_stored_items = []
    for run_no in range(1, times+1):
        log.info('Run #%2d', run_no)
        effective_mem, stored_items = execute_test(mc, minval, maxval, memlimit)
        all_effective_mem.append(effective_mem)
        all_stored_items.append(stored_items)
    return all_effective_mem, all_stored_items


def execute_test_for_values_range(range_name, minval, maxval):
    log.info('')
    # Run dataset on cachelot
    log.info('*** CACHELOT - range "%s" [%d:%d]' % (range_name, minval, maxval))
    cache_process = shell_exec(CACHELOTD + ' -p 11211')
    time.sleep(1) # ensure network is up
    mc = memcached.connect_tcp('localhost', 11211)
    cachelot_eff_mem, cachelot_items = execute_test_n_times(NUM_RUNS, mc, minval, maxval, MEMORY_LIMIT)
    cache_process.terminate()
    log.info('-' * 60)

    # Run dataset on memcached
    log.info('*** MEMCACHED - range "%s" [%d:%d]' % (range_name, minval, maxval))
    cache_process = shell_exec(MEMCACHED + ' -p 11212')
    time.sleep(1)
    mc = memcached.connect_tcp('localhost', 11212)
    memcached_eff_mem, memcached_items = execute_test_n_times(NUM_RUNS, mc, minval, maxval, MEMORY_LIMIT)
    cache_process.terminate()
    log.info('-' * 60)
    log.info('\n\n')
    r  = 'cachelot: [%s]\n' % ', '.join('%.02f' % toMb(mem) for mem in cachelot_eff_mem)
    r += 'memcached: [%s]\n' % ', '.join('%.02f' % toMb(mem) for mem in memcached_eff_mem)
    r += 'memDiff: [%s]\n' % ', '.join('%.02f' % toMb(abs(m1 - m2)) for m1, m2 in zip(cachelot_eff_mem, memcached_eff_mem))
    r += 'cachelotItems: [%s]\n' % ', '.join('%d' % i for i in cachelot_items)
    r += 'memcachedItems: [%s]\n' % ', '.join('%d' % i for i in memcached_items)
    return r


def main():
    benchmark_data = {}
    # collect data
    for range, minval, maxval in VALUE_RANGES:
        benchmark_data[range] = execute_test_for_values_range(range, minval, maxval)
    # print data
    print('\n\n\n')
    for range, minval, maxval in VALUE_RANGES:
        print('Range %d ~ %d bytes "%s"' % (minval, maxval, range))
        print(benchmark_data[range])
        print('\n')


if __name__ == '__main__':
    setup_logging()
    main()

#!/usr/bin/env python
#
# Cachelot server memory consumption measurement
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

SELF, _ = os.path.splitext(os.path.basename(sys.argv[0]))
BASEDIR = os.path.normpath(os.path.dirname(os.path.abspath(sys.argv[0])) + '/..')
CACHELOTD = os.path.join(BASEDIR, 'bin/cachelotd')
MEMCACHED = '/usr/bin/memcached'

KEY_ALPHABET = string.ascii_letters + string.digits

VALUE_RANGES = { 'small': (10, 250),
                 'medium': (250, 1024),
                 'large' :(1024, 100000),
                 'all': (10, 100000)}

MAX_DICT_MEM = 1024 * 1024 * 128  # 128Mb


FILTER_STATS = frozenset([
    'bytes',
    'bytes_read',
    'bytes_written',
    'cmd_get',
    'cmd_set',
    'curr_items',
    'evicted_unfetched',
    'evictions',
    'expired_unfetched',
    'get_hits',
    'get_misses',
    'hash_bytes',
    'hash_capacity',
    'hash_power_level',
    'limit_maxbytes',
    'num_alloc_errors',
    'num_free',
    'num_free_table_hits',
    'num_free_table_merges',
    'num_free_table_weak_hits',
    'num_malloc',
    'num_realloc',
    'num_realloc_errors',
    'num_used_table_hits',
    'num_used_table_merges',
    'num_used_table_weak_hits',
    'reclaimed',
    'set_existing',
    'set_new',
    'total_items',
    'total_realloc_requested',
    'total_realloc_served',
    'total_realloc_unserved',
    'total_requested',
    'total_served',
    'total_unserved',
    'uptime',
    'version'
])


def setup_logging():
    """
    Setup python logging module (destinations like terminal output and file, formatting, etc.)
    """
    # logging setup
    logging_default_level = logging.DEBUG
    logging_console_enable = True
    logging_console_format = '%(levelname)s: %(message)s'
    logging_file_enable = True
    logging_file_format = '%(asctime)s:%(levelname)s: %(message)s'
    logging_file_date_format = '%Y-%m-%d %H:%M:%S'
    logging_file_dir = './'
    logging_file_name = SELF + time.strftime('_%Y%b%d-%H%M%S') + '.log'


    logging.addLevelName(logging.FATAL, 'FATAL')
    logging.addLevelName(logging.ERROR, 'ERROR')
    logging.addLevelName(logging.WARN,  'WARNING')
    logging.addLevelName(logging.INFO,  'INFO')
    logging.addLevelName(logging.DEBUG, 'DEBUG')
    root_logger = logging.getLogger()
    # Console output
    if logging_console_enable:
        console = logging.StreamHandler()
        formatter = logging.Formatter(logging_console_format)
        console.setFormatter(formatter)
        root_logger.addHandler(console)
    # Log file
    if logging_file_enable:
        filename = os.path.join(logging_file_dir, logging_file_name)
        logfile = logging.FileHandler(filename)
        formatter = logging.Formatter(logging_file_format, logging_file_date_format)
        logfile.setFormatter(formatter)
        root_logger.addHandler(logfile)
    atexit.register(logging.shutdown)
    root_logger.setLevel(logging_default_level)


def random_key():
    length = random.randint(10, 35)
    return ''.join(random.choice(KEY_ALPHABET) for _ in range(length))


def random_value(minlen, maxlen):
    length = random.randint(minlen, maxlen)
    return ''.join(map(chr, (random.randint(0,255) for _ in xrange(length))))


def log_stats(mc):
    for stat, value in mc.stats():
        if stat in FILTER_STATS:
            log.debug('%30s:  %s' % (stat, value))


def shell_exec(command):
    devnull = open(os.devnull, 'w')
    return subprocess.Popen(args=shlex.split(command), stdout=devnull, stderr=devnull)


def create_kv_data(range_name):
    minval, maxval = VALUE_RANGES[range_name]
    current_dict_mem = 0
    in_memory_kv = []
    log.info('Creating KV data for the range "%s" [%d:%d]' %(range_name, minval, maxval))
    start_time = time.time()
    while current_dict_mem < MAX_DICT_MEM:
        k = random_key()
        current_dict_mem += len(k)
        v = random_value(minval, maxval)
        current_dict_mem += len(v)
        in_memory_kv.append( (k, v) )
    took_time = time.time() - start_time
    log.debug('  Took: %.2f sec', took_time)
    log.info('Local effective memory: %d (%d items).', current_dict_mem, len(in_memory_kv))

    return in_memory_kv


def execute_test(mc, kv_data):
    for run_no in range(5):
        start_time = time.time()
        log.debug('Fill-in caching server ...')
        for k, v in kv_data:
            mc.set(k, v)
        log.debug('  Took: %.2f sec', time.time() - start_time)
        log.debug('Checking caching server ...')
        num_items = 0
        effective_memory = 0
        start_time = time.time()
        for k, local_val in kv_data:
            external_val = mc.get(k)
            if not external_val:
                # item was evicted, move along
                continue
            num_items += 1
            effective_memory += len(k) + len(local_val)

        log.info('External effective memory: %d (%d items).', effective_memory, num_items)
        log.debug('  Took: %.2f sec', time.time() - start_time)

        # print statistics
        log_stats(mc)
        random.shuffle(kv_data)


def execute_test_for_values_range(range_name):
    log.info('*' * 60)
    # Create test data
    in_memory_kv = create_kv_data(range_name)

    # Run dataset on cachelot
    log.info("*** CACHELOT")
    cache_process = shell_exec(CACHELOTD)
    time.sleep(3)
    mc = memcached.connect_tcp('localhost', 11211)
    execute_test(mc, in_memory_kv)
    cache_process.terminate()
    time.sleep(2)
    log.info('*' * 60)

    # Run dataset on memcached
    log.info("*** MEMCACHED")
    cache_process = shell_exec(MEMCACHED + ' -p 11212 -n 32 -f 1.05')
    time.sleep(3)
    mc = memcached.connect_tcp('localhost', 11212)
    execute_test(mc, in_memory_kv)
    cache_process.terminate()
    time.sleep(2)
    log.info('*' * 60)
    log.info('\n\n')


def main():
    for range in ['medium', 'small', 'large', 'all']:
      execute_test_for_values_range(range)


if __name__ == '__main__':
    setup_logging()
    main()

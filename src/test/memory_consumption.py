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
CACHELOTD = './bin/cachelotd'
MEMCACHED = '/usr/bin/memcached'

KEY_ALPHABET = string.ascii_letters + string.digits

VALUE_RANGES = { 'small': (10, 250),
                 'medium': (250, 1024), 
                 'large' :(1024, 100000),
                 'all': (10, 100000)}

MAX_DICT_MEM = 1024 * 1024 * 1024 * 4  # 4Gb
MAX_DICT_MEM = 1024 * 1024 * 1

INTERESTING_STATS = frozenset([ 
  'hash_capacity',
  'curr_items',
  'hash_is_expanding',
  'num_malloc',
  'num_free',
  'num_realloc',
  'num_alloc_errors',
  'num_realloc_errors',
  'total_requested',
  'total_served',
  'total_unserved',
  'total_realloc_requested',
  'total_realloc_served',
  'total_realloc_unserved',
  'num_free_table_hits',
  'num_free_table_weak_hits',
  'num_free_table_merges',
  'num_used_table_hits',
  'num_used_table_weak_hits',
  'num_used_table_merges',
  'limit_maxbytes',
  'evictions' ])


def setup_logging():
    """
    Setup python logging module (destinations like terminal output and file, formatting, etc.)
    Note: This is low-level setup, to change logging output format see config.py instead
    """
    # logging setup
    logging_default_level = logging.DEBUG
    logging_console_enable = True
    logging_console_format = '%(levelname)s: %(message)s'
    logging_file_enable = True
    logging_file_format = '%(asctime)s:%(levelname)s: %(message)s'
    logging_file_date_format = '%Y %b %d %H:%M:%S'
    logging_file_dir = './'
    logging_file_name = SELF + time.strftime('%Y%b%d-%H%M%S') + '.log'


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
    length = random.randint(10, 250)
    return ''.join(random.choice(KEY_ALPHABET) for _ in range(length))
    

def random_value(minlen, maxlen):
    length = random.randint(minlen, maxlen)
    return ''.join(map(chr, (random.randint(0,255) for _ in xrange(length))))


def log_stats(mc):
    for stat, value in mc.stats():
        #if stat in INTERESTING_STATS:
            log.info('%30s:  %s' % (stat, value))


def shell_exec(command):
    devnull = open(os.devnull, 'w')
    return subprocess.Popen(args=shlex.split(command), stdout=devnull, stderr=devnull)


def create_kv_data(range_name):
    minval, maxval = VALUE_RANGES[range_name]
    current_dict_mem = 0
    in_memory_kv = []
    log.info('*' * 45)
    log.info('*** Test for values in range "%s" [%d:%d]' %(range_name, minval, maxval))
    start_time = time.time()
    while current_dict_mem < MAX_DICT_MEM:
        k = random_key()
        current_dict_mem += len(k)
        v = random_value(minval, maxval)
        current_dict_mem += len(v)
        in_memory_kv.append( (k, v) )
    took_time = time.time() - start_time
    log.info('Effective memory: %d (%d items).', current_dict_mem, len(in_memory_kv))
    log.debug('Took: %f sec', took_time)
    return in_memory_kv


def execute_test(mc, kv_data):
    for run_no in range(5):
        start_time = time.time()
        for k, v in kv_data:
            mc.set(k, v)
        log.info('Fill data in the cache took %f sec.', time.time() - start_time)
        log_stats(mc)


def execute_test_for_values_range(range_name):

    # Create test data
    in_memory_kv = create_kv_data(range_name)

    # Run dataset on cachelot
    cache_process = shell_exec(CACHELOTD)
    time.sleep(3)
    mc = memcached.connect_tcp('localhost', 11211)
    execute_test(mc, in_memory_kv)
    cache_process.terminate()
    time.sleep(2)
    log.info('*' * 60)

    # Run dataset on memcached
    cache_process = shell_exec(MEMCACHED + ' -p 11212')
    time.sleep(3)
    mc = memcached.connect_tcp('localhost', 11212)
    execute_test(mc, in_memory_kv)
    cache_process.terminate()
    time.sleep(2)


def main():
    for range in VALUE_RANGES:
      execute_test_for_values_range(range)


if __name__ == '__main__':
    setup_logging()
    main()

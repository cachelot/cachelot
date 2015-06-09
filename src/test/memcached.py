import socket
import logging
import os
import errno
import cPickle as pickle
from cStringIO import StringIO

log = logging.getLogger(__name__)

CMD_TERMINATOR = '\r\n'

class ConnectionAbortedException(socket.error):
    def __init__(self):
        # Software caused connection abort
        socket.error.__init__(self, errno.ECONNABORTED, os.strerror(errno.ECONNABORTED))

class KeyNotFoundError(Exception):
    pass

class Error(Exception):
    def __init__(self, msg):
        self.message = msg
    def __str__(self):
        return self.message

class ClientError(Error):
    def __init__(self, msg):
        Error.__init__(self, msg)

class ServerError(Error):
    def __init__(self, msg):
        Error.__init__(self, msg)


class Client(object):
    BIT = lambda bit_no: 1 << (bit_no - 1)
    __FLAG_NONE = 0
    __FLAG_VALUE_INTEGER = BIT(1)
    __FLAG_VALUE_PICKLED = BIT(2)

    def __init__(self):
        log.debug('create memcached client')
        self._sock = None
        self.__memcache_interface = None
        self.__socket_family = None
        self.__socket_type = None

    def setup_unix_socket(self, socket_name):
        log.debug('setup: unix socket "%s"', socket_name)
        self.__memcache_interface = socket_name
        self.__socket_family = socket.AF_UNIX
        self.__socket_type = socket.SOCK_STREAM

    def setup_tcp_socket(self, host, port):
        log.debug('setup tcp: %s:%s', str(host), str(port))
        self.__memcache_interface = (host, port)
        self.__socket_family = socket.AF_INET
        self.__socket_type = socket.SOCK_STREAM

    def connect(self):
        assert self.__memcache_interface, 'Not configured'
        try:
            log.info('Connecting to "%s"...', self.__memcache_interface)
            self._sock = socket.socket(self.__socket_family, self.__socket_type)
            self._socket_buffer = ''
            self._sock.connect(self.__memcache_interface)
            log.info('Connected successfully')
        except:
            self._sock = None
            raise

    def is_connected(self):
        return self._sock != None

    def close(self):
        try:
            if self.is_connected():
                log.info('Closing connection to memcached ...')
                self._sock.shutdown()
                self._sock.close()
                log.info('Connection was closed')
        except Exception as e:
            log.error('Failed to properly close connection to memcached: (%s)', str(e))
        finally:
            self._sock = None

    def get(self, key):
        value = None
        for rkey, value, _ in self.__retrieve('get', [key]):
            pass
        return value

    def gets(self, key):
        value = None
        cas_unique = None
        for rkey, value, cas_unique in self.__retrieve('gets', [key]):
            pass
        return (value, cas_unique)

    def get_multi(self, keys):
        for rkey, value, _ in self.__retrieve('get', keys):
            yield (rkey, value)

    def gets_multi(self, keys):
        for rkey, value, cas_unique in self.__retrieve('gets', keys):
            yield (rkey, value, cas_unique)


    def add(self, key, value, expire_time=0):
        return self.__store('add', key, value, expire_time) == 'STORED'

    def set(self, key, value, expire_time=0):
        self.__store('set', key, value, expire_time)

    def replace(self, key, value, expire_time=0):
        return self.__store('replace', key, value, expire_time) == 'STORED'

    def cas(self, key, value, expire_time, cas_unique):
        reply = self.__store('cas', key, value, expire_time, cas_unique)
        if reply == 'STORED':
            return True
        elif reply == 'EXISTS':
            return False
        else:
            assert reply == 'NOT_STORED', 'Invalid reply'
            raise KeyNotFoundError()

    def incr(self, key, increment):
        return self.__arithmetic('incr', key, increment)

    def decr(self, key, decrement):
        return self.__arithmetic('decr', key, decrement)

    def delete(self, key):
        command_str = "delete %(key)s\r\n" % locals()
        self.__send(command_str)
        response = self.__receive_line()
        self.__raise_if_errors(response)
        return self.__expect(response, ['DELETED', 'NOT_FOUND']) == 'DELETED'

    def touch(self, key, seconds):
        command_str = "touch %(key)s %(seconds)d\r\n" % locals()
        self.__send(command_str)
        response = self.__receive_line()
        self.__raise_if_errors(response)
        return self.__expect(response, ['TOUCHED', 'NOT_FOUND']) == 'TOUCHED'

    def stats(self, stats_type=None):
        if stats_type:
            self.__send('stats %(stats_type)s\r\n' % locals())
        else:
            self.__send('stats\r\n')
        response = self.__receive_line()
        self.__raise_if_errors(response)
        while response.startswith('STAT'):
            response = response.split(' ')
            stat_name, stat_value = response[1], response[2]
            yield (stat_name, stat_value)
            response = self.__receive_line()
        self.__expect(response, 'END')

    def __retrieve(self, command, keys):
        assert command in ['get', 'gets'],  'unsupported command: ' + command
        command_str = command + ' ' + ' '.join(keys) + '\r\n'
        self.__send(command_str)
        response = self.__receive_line()
        self.__raise_if_errors(response)
        num_found = 0
        while response.startswith('VALUE'):
            num_found += 1
            response = response.split(' ')
            ret_key, ret_flags, ret_length = response[1], int(response[2]), int(response[3])
            if command == 'gets':
                ret_cas = int(response[4])
            else:
                ret_cas = None
            data = self.__receive(ret_length)
            self.__expect(self.__receive_line(), '') # remove "\r\n" from receive buffer
            response = self.__receive_line()
            yield (ret_key, self.__deserialize(ret_flags, data), ret_cas)
        self.__expect(response, 'END')

    def __store(self, command, key, value, expire_time, cas_unique=None):
        assert command in ['set', 'add', 'replace', 'cas'], 'unsupported command: ' + command
        flags, data = self.__serialize(value)
        data_length = len(data)
        command_str = '%(command)s %(key)s %(flags)d %(expire_time)d %(data_length)d'
        if command == 'cas':
            command_str += ' %(cas_unique)d'
        command_str += '\r\n%(data)s\r\n'
        command_str %= locals()
        # communicate
        self.__send(command_str)
        response = self.__receive_line()
        self.__raise_if_errors(response)
        return self.__expect(response, ['STORED', 'NOT_STORED', 'EXISTS'])

    def __arithmetic(self, command, key, value):
        assert command in ['incr', 'decr'], 'unsupported command: ' + command
        command_str = "%(command)s %(key)s %(value)d\r\n" % locals()
        self.__send(command_str)
        response = self.__receive_line()
        self.__raise_if_errors(response)
        return int(response)

    def __serialize(self, value):
        flags = Client.__FLAG_NONE
        result = None
        if isinstance(value, str):
            result = value
        elif isinstance(value, int):
            flags |= Client.__FLAG_VALUE_INTEGER
            result = str(value)
        else:
            flags |= Client.__FLAG_VALUE_PICKLED
            result = pickle.dumps(value, pickle.HIGHEST_PROTOCOL)
        return flags, result

    def __deserialize(self, flags, data):
        if flags == Client.__FLAG_NONE:
            return data
        elif flags & Client.__FLAG_VALUE_INTEGER:
            return int(data)
        elif flags & Client.__FLAG_VALUE_PICKLED:
            return pickle.loads(data)
        else:
            assert False, 'Unknown flags'

    def __send(self, cmd):
        assert self.is_connected(), 'Not connected'
        try:
            self._sock.sendall(cmd)
        except:
            self._sock = None
            raise

    def __receive_line(self):
        assert self.is_connected(), 'Not connected'
        try:
            term_pos = self._socket_buffer.find(CMD_TERMINATOR)
            while term_pos == -1:
                chunk = self._sock.recv(4096)
                if not chunk:
                    raise ConnectionAbortedException()
                self._socket_buffer += chunk
                term_pos = self._socket_buffer.find(CMD_TERMINATOR)
            result = self._socket_buffer[:term_pos]
            self._socket_buffer = self._socket_buffer[term_pos + len(CMD_TERMINATOR):]
            return result
        except:
            self._sock = None
            raise

    def __receive(self, size):
        assert self.is_connected(), 'Not connected'
        try:
            while len(self._socket_buffer) < size:
                chunk = self._sock.recv(4096)
                if not chunk:
                    raise ConnectionAbortedException()
                self._socket_buffer += chunk
            result = self._socket_buffer[:size]
            self._socket_buffer = self._socket_buffer[size:]
            return result
        except:
            self._sock = None
            raise

    def __raise_if_errors(self, response):
        def check_for_specific_error(err_type, exc_class):
            if response.startswith(err_type.upper()):
                errmsg = response[len(err_type):].strip()
                raise exc_class(errmsg)
        check_for_specific_error('ERROR', Error)
        check_for_specific_error('CLIENT_ERROR', ClientError)
        check_for_specific_error('SERVER_ERROR', ServerError)
        check_for_specific_error('NOT_FOUND', KeyNotFoundError)
        return response

    def __expect(self, response, expected):
        txt_ = lambda s: s if s else '<nothing>'
        if isinstance(expected, str):
            if response != expected:
                errmsg = 'Expected "%s" server command, got "%s" instead' % (txt_(expected), txt_(response))
                raise Error(errmsg)
        else:
            assert isinstance(expected, list) and len(expected) > 1, 'Invalid arguments in "__expect"'
            if not response in expected:
                errmsg = 'Expected one of: [ ' + '"%s" ' * len(expected) + '] server commands, got "%s" instead'
                errmsg %= tuple(map(txt_, expected + [response]))
                raise Error(errmsg)
        return response


def unix_connect(socket_name):
    """ return Client connected to the Unix domain socket """
    c = Client()
    c.setup_unix_socket(socket_name)
    c.connect()
    return c


def connect_tcp(host, port):
    """ return Client connected to the TCP socket """
    c = Client()
    c.setup_tcp_socket(host, port)
    c.connect()
    return c

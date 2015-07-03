set (CACHELOT_SERVER_SOURCES
        stream_connection.h
        io_buffer.h
        main.cpp
        network.h
        settings.cpp
        settings.h
        stream_server.h
        datagram_server.h
        memcached/error.h
        memcached/proto_ascii.h
        memcached/proto_ascii.cpp
        memcached/proto_binary.h
        memcached/servers.h
        memcached/conversation.h
        memcached/memcached.h
        memcached/memcached.cpp
)

add_executable (cachelotd ${CACHELOT_SERVER_SOURCES})
target_link_libraries (cachelotd cachelot ${Boost_LIBRARIES})

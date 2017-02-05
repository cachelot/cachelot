set (CACHELOT_SERVER_SOURCES
        io_buffer.h
        network.h
        socket_stream.h
        socket_datagram.h
        settings.h
        settings.cpp
        compression.h
        compression.cpp
        memcached/error.h
        memcached/proto_defs.h
        memcached/proto_ascii.h
        memcached/proto_ascii.cpp
        memcached/proto_binary.h
        memcached/proto_binary.cpp
        memcached/servers.h
        memcached/conversation.h
        memcached/memcached.h
        memcached/memcached.cpp
        main.cpp
)

add_executable (cachelotd ${CACHELOT_SERVER_SOURCES} ${LZ4_SOURCES})
target_link_libraries (cachelotd cachelot ${Boost_LIBRARIES} ${Blosc_LIBRARIES})

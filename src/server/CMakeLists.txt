set (CACHELOT_SERVER_SOURCES
        conn_stream.h
        io_buffer.h
        io_serialization.h
        main.cpp
        network.h
        settings.cpp
        settings.h
        server_tcp.h
        server_local.h
        memcached/memcached.h
        memcached/proto_ascii.h
        memcached/proto_text.h
        memcached/servers.h
        memcached/conversation.h
        memcached/conversation.cpp
)

add_executable (cachelotd ${CACHELOT_SERVER_SOURCES})
target_link_libraries (cachelotd cachelot ${Boost_LIBRARIES})

set (CACHELOT_SERVER_SOURCES
        async_connection.h
        io_buffer.h
        io_serialization.h
        main.cpp
        network.h
        settings.cpp
        settings.h
        server_tcp.h
        server_local.h
        memcached/memcached.h
        memcached/proto_text.h
        memcached/servers.h
)

add_executable (cachelotd ${CACHELOT_SERVER_SOURCES})
target_link_libraries (cachelotd cachelot ${Boost_LIBRARIES})

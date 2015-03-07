# ${CMAKE_CURRENT_LIST_DIR} allows to include this files anywhere in the project
# For instance it is included in unit tests to simplify linking

set (CACHELOT_SOURCES
                ${CMAKE_CURRENT_LIST_DIR}/cachelot.h
                ${CMAKE_CURRENT_LIST_DIR}/cachelot.cpp
                ${CMAKE_CURRENT_LIST_DIR}/bits.h
                ${CMAKE_CURRENT_LIST_DIR}/string_conv.h
                ${CMAKE_CURRENT_LIST_DIR}/hash_table.h
                ${CMAKE_CURRENT_LIST_DIR}/bytes.h
                ${CMAKE_CURRENT_LIST_DIR}/error.h
                ${CMAKE_CURRENT_LIST_DIR}/error.cpp
                ${CMAKE_CURRENT_LIST_DIR}/dict.h
                ${CMAKE_CURRENT_LIST_DIR}/hash_fnv1a.h
                ${CMAKE_CURRENT_LIST_DIR}/random.h
                ${CMAKE_CURRENT_LIST_DIR}/io_buffer.h
                ${CMAKE_CURRENT_LIST_DIR}/io_serialization.h
                ${CMAKE_CURRENT_LIST_DIR}/settings.h
                ${CMAKE_CURRENT_LIST_DIR}/settings.cpp
                ${CMAKE_CURRENT_LIST_DIR}/memalloc.h
                ${CMAKE_CURRENT_LIST_DIR}/memalloc.inl
                ${CMAKE_CURRENT_LIST_DIR}/xrange.h
                ${CMAKE_CURRENT_LIST_DIR}/expiration_clock.h
                ${CMAKE_CURRENT_LIST_DIR}/item.h
                ${CMAKE_CURRENT_LIST_DIR}/cache.h
                ${CMAKE_CURRENT_LIST_DIR}/cache_service.h
                ${CMAKE_CURRENT_LIST_DIR}/cache_service.cpp
                ${CMAKE_CURRENT_LIST_DIR}/mpsc_queue.h
                ${CMAKE_CURRENT_LIST_DIR}/network.h
                ${CMAKE_CURRENT_LIST_DIR}/async_connection.h
                ${CMAKE_CURRENT_LIST_DIR}/tcp_server.h
                ${CMAKE_CURRENT_LIST_DIR}/unix_socket_server.h
                ${CMAKE_CURRENT_LIST_DIR}/proto_memcached.h
                ${CMAKE_CURRENT_LIST_DIR}/proto_memcached_text.h
                ${CMAKE_CURRENT_LIST_DIR}/proto_memcached_servers.h
                ${CMAKE_CURRENT_LIST_DIR}/pool_allocator.h
                ${CMAKE_CURRENT_LIST_DIR}/atom.h
                ${CMAKE_CURRENT_LIST_DIR}/actor.cpp
                ${CMAKE_CURRENT_LIST_DIR}/actor.h
                ${CMAKE_CURRENT_LIST_DIR}/thread_safe_cache.h
                ${CMAKE_CURRENT_LIST_DIR}/thread_safe_cache.cpp
        )


# ${CMAKE_CURRENT_LIST_DIR} allows to use this files anywhere in the project
# For instance cachelot and unit tests shares same set of source files

set (CACHELOT_SOURCES
                ${CMAKE_CURRENT_LIST_DIR}/cachelot.h
                ${CMAKE_CURRENT_LIST_DIR}/cachelot.cpp
                ${CMAKE_CURRENT_LIST_DIR}/bits.h
                ${CMAKE_CURRENT_LIST_DIR}/string_conv.h
                ${CMAKE_CURRENT_LIST_DIR}/hash_table.h
                ${CMAKE_CURRENT_LIST_DIR}/bytes.h
                ${CMAKE_CURRENT_LIST_DIR}/error.h
                ${CMAKE_CURRENT_LIST_DIR}/error.cpp
                ${CMAKE_CURRENT_LIST_DIR}/item.h
                ${CMAKE_CURRENT_LIST_DIR}/dict.h
                ${CMAKE_CURRENT_LIST_DIR}/hash_fnv1a.h
                ${CMAKE_CURRENT_LIST_DIR}/settings.h
                ${CMAKE_CURRENT_LIST_DIR}/settings.cpp
        )

#if (CMAKE_BUILD_TYPE MATCHES DEBUG)
#        LIST(APPEND ${VAULT_SOURCES} ${VAULT_DEBUG_SOURCES})
#endif ()

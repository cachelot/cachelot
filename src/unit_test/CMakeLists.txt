project (unit_tests)

set (CACHELOT_UNIT_TEST_SOURCES
                unit_test.cpp
                unit_test.h
                test_bits.cpp
                test_string_conv.cpp
                test_slice.cpp
                test_item.cpp
                test_hash_table.cpp
                test_dict.cpp
                test_intrusive_list.cpp
                test_memalloc.cpp
                test_stats.cpp
                test_cache.cpp
                test_cache_stats.cpp
                test_io_buffer.cpp
        )

add_executable (unit_tests ${CACHELOT_UNIT_TEST_SOURCES})
target_link_libraries (unit_tests cachelot ${Boost_LIBRARIES})


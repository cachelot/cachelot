project (unit_tests)

include (${CMAKE_SOURCE_DIR}/src/cachelot/CMakeLists.txt)

set (CACHELOT_UNIT_TEST_SOURCES
                unit_test.cpp
                unit_test.h
                test_bits.cpp
                test_string_conv.cpp
                test_bytes.cpp
                test_item.cpp
                ${CACHELOT_SOURCES} 
        )

list (REMOVE_ITEM CACHELOT_UNIT_TEST_SOURCES main.cpp) # remove cachelot executable entry point

add_executable (unit_tests ${CACHELOT_UNIT_TEST_SOURCES})
set_target_properties (unit_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
target_link_libraries (unit_tests ${Boost_LIBRARIES})

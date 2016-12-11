project (test_c_api)

add_executable (test_c_api test_c_api.c)
target_link_libraries (test_c_api cachelot ${Boost_LIBRARIES})

if(BUILD_TESTS_SHARED)
    add_executable(test_crypto test_crypto.cpp)
    target_link_libraries(test_crypto PRIVATE shared)
endif()

#if(BUILD_TESTS_TURN)
#    add_executable(test_turn test_turn.cpp)
#    target_link_libraries(test_turn PRIVATE shared turn_server)
#endif()

if(BUILD_TESTS_BACKEND)
    add_executable(test_backend test_backend.cpp)
    target_include_directories(test_backend PRIVATE
        ${CMAKE_SOURCE_DIR}/backend
    )
    target_link_libraries(test_backend PRIVATE shared backend)
endif()

#if(BUILD_TESTS_CLIENT)
#    add_executable(test_client test_client.cpp)
#    target_link_libraries(test_client PRIVATE shared client_cli)
#endif()
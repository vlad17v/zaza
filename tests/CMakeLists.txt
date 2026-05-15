if(BUILD_TESTS_SHARED)
    add_executable(test_crypto test_crypto.cpp)
    target_link_libraries(test_crypto PRIVATE shared)
endif()

if(BUILD_TESTS_BACKEND)
    add_executable(test_backend test_backend.cpp)
    target_include_directories(test_backend PRIVATE
        ${CMAKE_SOURCE_DIR}/backend
        ${CMAKE_SOURCE_DIR}/shared
    )
    target_link_libraries(test_backend PRIVATE shared backend)
endif()

if(BUILD_TESTS_TURN)
    add_executable(test_turn test_turn.cpp)
    target_include_directories(test_turn PRIVATE
        ${CMAKE_SOURCE_DIR}/turn_server/src
    )
    target_link_libraries(test_turn PRIVATE
        shared
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_sources(test_turn PRIVATE
        ${CMAKE_SOURCE_DIR}/turn_server/src/message/parser.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/message/builder.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/auth/nonce_manager.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/auth/hmac_validator.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/auth/long_term_cred.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/allocation/permission_table.cpp
        ${CMAKE_SOURCE_DIR}/turn_server/src/allocation/allocation_manager.cpp
    )
endif()
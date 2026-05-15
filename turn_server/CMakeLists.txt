add_executable(turn_server
    src/main.cpp
    src/transport/udp_transport.cpp
    src/transport/tcp_transport.cpp
    src/transport/tls_transport.cpp
    src/transport/dtls_transport.cpp
    src/message/parser.cpp
    src/message/builder.cpp
    src/auth/nonce_manager.cpp
    src/auth/hmac_validator.cpp
    src/auth/long_term_cred.cpp
    src/allocation/permission_table.cpp
    src/allocation/allocation_manager.cpp
)

target_include_directories(turn_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(turn_server PRIVATE
    shared
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)
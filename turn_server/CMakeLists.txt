add_executable(turn_server
    src/main.cpp
    src/transport/udp_transport.cpp
    src/transport/tcp_transport.cpp
    src/message/parser.cpp
    src/message/builder.cpp
)

target_include_directories(turn_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(turn_server PRIVATE
    shared
    Boost::system
)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(client_cli
    src/main.cpp
    src/cli/parser.cpp
    src/cli/commands.cpp
    src/cli/repl.cpp
    src/http/http_client.cpp
    src/signaling/ws_client.cpp
    src/signaling/message_handler.cpp
)

target_include_directories(client_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(client_cli PRIVATE
    shared
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(BUILD_TESTS_CLIENT)
    add_executable(test_client test_client.cpp)
    target_include_directories(test_client PRIVATE
        ${CMAKE_SOURCE_DIR}/client_cli/src
    )
    target_link_libraries(test_client PRIVATE
        shared
        nlohmann_json::nlohmann_json
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_sources(test_client PRIVATE
        ${CMAKE_SOURCE_DIR}/client_cli/src/cli/parser.cpp
        ${CMAKE_SOURCE_DIR}/client_cli/src/cli/commands.cpp
        ${CMAKE_SOURCE_DIR}/client_cli/src/cli/repl.cpp
        ${CMAKE_SOURCE_DIR}/client_cli/src/http/http_client.cpp
        ${CMAKE_SOURCE_DIR}/client_cli/src/signaling/message_handler.cpp
        ${CMAKE_SOURCE_DIR}/client_cli/src/audio/wav.cpp
    )
endif()
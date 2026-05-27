add_library(backend
    auth/jwt.cpp
    auth/auth_service.cpp
    auth/user_store.cpp
    api/http_server.cpp
    api/routes.cpp
    signaling/ws_server.cpp
    signaling/router.cpp
    calls/call_manager.cpp
)

target_include_directories(backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(backend PUBLIC
    shared
    jwt-cpp::jwt-cpp
    nlohmann_json::nlohmann_json
    SQLite3::SQLite3
    OpenSSL::Crypto
    OpenSSL::SSL
    Boost::system
)

add_executable(backend_server
    main.cpp
)

target_include_directories(backend_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(backend_server PRIVATE
    backend
)
add_library(backend
    auth/jwt.cpp
    auth/auth_service.cpp
    auth/user_store.cpp
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
)
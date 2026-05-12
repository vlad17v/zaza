add_library(shared INTERFACE)

target_include_directories(shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(OpenSSL REQUIRED)

target_link_libraries(shared INTERFACE
    OpenSSL::Crypto
)
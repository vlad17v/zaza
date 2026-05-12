cmake_minimum_required(VERSION 3.16)
project(chat_project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
endif()

option(BUILD_TESTS         "Собрать все тесты"             OFF)
option(BUILD_TESTS_SHARED  "Собрать тесты shared/"         OFF)
option(BUILD_TESTS_TURN    "Собрать тесты turn_server/"    OFF)
option(BUILD_TESTS_BACKEND "Собрать тесты backend/"        OFF)
option(BUILD_TESTS_CLIENT  "Собрать тесты client_cli/"     OFF)

if(BUILD_TESTS)
    set(BUILD_TESTS_SHARED  ON)
    set(BUILD_TESTS_TURN    ON)
    set(BUILD_TESTS_BACKEND ON)
    set(BUILD_TESTS_CLIENT  ON)
endif()

add_subdirectory(shared)
add_subdirectory(backend)
add_subdirectory(turn_server)
add_subdirectory(client_cli)

if(BUILD_TESTS OR BUILD_TESTS_SHARED OR BUILD_TESTS_TURN
               OR BUILD_TESTS_BACKEND OR BUILD_TESTS_CLIENT)
    add_subdirectory(tests)
endif()
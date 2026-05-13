cmake_minimum_required(VERSION 3.16)
project(chat_project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
endif()

find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
    set_target_properties(SQLite3::SQLite3 PROPERTIES
        IMPORTED_LOCATION             "${SQLite3_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}"
    )
endif()
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

include(FetchContent)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

option(BUILD_TESTS         "Собрать все тесты"          OFF)
option(BUILD_TESTS_SHARED  "Собрать тесты shared/"      OFF)
option(BUILD_TESTS_TURN    "Собрать тесты turn_server/" OFF)
option(BUILD_TESTS_BACKEND "Собрать тесты backend/"     OFF)
option(BUILD_TESTS_CLIENT  "Собрать тесты client_cli/"  OFF)

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
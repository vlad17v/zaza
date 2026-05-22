add_executable(client_cli
    src/main.cpp
    src/cli/parser.cpp
    src/cli/commands.cpp
    src/cli/repl.cpp
)

target_include_directories(client_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
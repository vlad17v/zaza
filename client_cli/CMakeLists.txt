find_package(Boost    REQUIRED)
find_package(OpenSSL  REQUIRED)
find_package(nlohmann_json REQUIRED)

find_library(DATACHANNEL_LIB datachannel
    PATHS /usr/local/lib /usr/lib)
find_path(DATACHANNEL_INCLUDE rtc/rtc.hpp
    PATHS /usr/local/include /usr/include)

if(NOT DATACHANNEL_LIB)
    message(FATAL_ERROR "libdatachannel not found")
endif()

add_executable(client_cli
    src/main.cpp
    src/cli/parser.cpp
    src/cli/commands.cpp
    src/cli/repl.cpp
    src/http/http_client.cpp
    src/signaling/ws_client.cpp
    src/signaling/message_handler.cpp
    src/rtc/peer_connection.cpp
    src/rtc/sdp_handler.cpp
    src/rtc/ice_handler.cpp
    src/audio/wav.cpp
    src/audio/capture.cpp
    src/audio/playback.cpp
)

target_include_directories(client_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DATACHANNEL_INCLUDE}
)

target_link_libraries(client_cli PRIVATE
    shared
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ${DATACHANNEL_LIB}
    pthread
)

cmake_minimum_required(VERSION 3.16)
project(RedisClone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Common code
set(COMMON_SRC
    src/common.cpp
)

# Clients
add_executable(client
    src/client/03_client.cpp
    ${COMMON_SRC}
)

add_executable(client_08
    src/client/03_client.cpp
    ${COMMON_SRC}
)

add_executable(client_tmp
    src/client/03_client.cpp
    ${COMMON_SRC}
)

# Servers
add_executable(server
    src/server/03_server.cpp
    ${COMMON_SRC}
)

add_executable(server_06
    src/server/06_server.cpp
    ${COMMON_SRC}
)

add_executable(server_08
    src/server/08_server.cpp
    ${COMMON_SRC}
)

add_executable(test_avl
    tests/test_avl.cpp
    src/ds/avl.cpp
)

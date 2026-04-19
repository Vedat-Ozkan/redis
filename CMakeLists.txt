cmake_minimum_required(VERSION 3.16)
project(RedisClone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(COMMON_SRC
    src/common.cpp
)

add_executable(client
    src/client/client.cpp
    ${COMMON_SRC}
)

add_executable(server
    src/server/server.cpp
    src/server/conn.cpp
    src/server/protocol.cpp
    src/server/kvstore.cpp
    src/ds/avl.cpp
    src/ds/zset.cpp
    src/ds/buffer.cpp
    src/ds/hashtable.cpp
    src/ds/thread_pool.cpp
    src/ds/ttl_heap.cpp
    ${COMMON_SRC}
)

add_executable(test_avl
    tests/test_avl.cpp
    src/ds/avl.cpp
)

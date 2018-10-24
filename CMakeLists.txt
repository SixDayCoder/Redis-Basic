CMAKE_MINIMUM_REQUIRED(VERSION 3.12)
PROJECT(redis_basic C)
SET(CMAKE_C_STANDARD 99)
INCLUDE_DIRECTORIES(
        main
        memory
        type
        object
        utlis
        network
        redis
)

SET(
        SRC_FILES
        main/main.c
        redis/redis.c
        memory/memory.c
        utlis/utlis.c
        type/sds.c
        type/list.c
        type/dict.c
        type/hash.c
        type/skiplist.c
        type/intset.c
        type/ziplist.c
        network/event.c
        network/select.c
        network/network.c
        network/handler.c
        object/object.c
        object/object_zset.c
        object/object_list.c
        object/object_string.c
        object/object_hash.c
        object/object_set.c)

ADD_EXECUTABLE(redis_basic ${SRC_FILES})
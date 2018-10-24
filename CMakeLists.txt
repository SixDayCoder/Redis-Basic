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
ADD_EXECUTABLE(
        redis_basic
        main/main.c
        memory/memory.h memory/memory.c
        type/sds.h      type/sds.c
        type/list.h     type/list.c
        type/dict.h     type/dict.c
        type/hash.h     type/hash.c
        type/skiplist.h type/skiplist.c
        type/intset.h   type/intset.c
        type/ziplist.h  type/ziplist.c
        object/object.h object/object.c
        utlis/utlis.h   utlis/utlis.c
        network/event.h   network/event.c network/select.c
        network/network.h network/network.c
        network/handler.h network/handler.c
        redis/redis.h redis/redis.c)
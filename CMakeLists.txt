CMAKE_MINIMUM_REQUIRED(VERSION 3.12)
PROJECT(redis_basic C)
SET(CMAKE_C_STANDARD 99)
INCLUDE_DIRECTORIES(main)
INCLUDE_DIRECTORIES(memory)
INCLUDE_DIRECTORIES(type)
INCLUDE_DIRECTORIES(object)
INCLUDE_DIRECTORIES(utlis)
ADD_EXECUTABLE(
        redis_basic
        main/main.c
        memory/memory.h memory/memory.c
        type/sds.h  type/sds.c
        type/list.h type/list.c
        type/dict.h type/dict.c
        type/hash.h type/hash.c
        type/skiplist.h type/skiplist.c
        type/intset.h type/intset.c
        object/object.h object/object.c
        utlis/utlis.h utlis/utlis.c)
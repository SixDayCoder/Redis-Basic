//
// Created by Administrator on 2018/9/25.
//

#ifndef REDIS_BASIC_MEMORY_H
#define REDIS_BASIC_MEMORY_H

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define  ZMALLOC_PREFIX_SIZE (sizeof(size_t))

static void zmalloc_oom_handler_default(size_t size)
{
    //%zu is for size_t
    fprintf(stderr, "zmalloc : out pf memory trying to allocate %zu bytes\n", size);
    fflush(stderr);
    abort();
}

//out of memory
static void (*zmalloc_oom_handler)(size_t) = zmalloc_oom_handler_default;

//不初始化分配的内存
void* zmalloc(size_t size);

//初始化分配的内存为0
void* zcalloc(size_t size);

//重新分配内存
void* zrealloc(void *ptr, size_t size);

//释放内存
void  zfree(void* ptr);

#endif //REDIS_BASIC_MEMORY_H

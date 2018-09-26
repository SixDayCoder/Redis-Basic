//
// Created by Administrator on 2018/9/26.
//

#include "memory.h"

void* zmalloc(size_t size)
{
    void *ptr = malloc(size + ZMALLOC_PREFIX_SIZE);
    if(!ptr)
    {
        zmalloc_oom_handler(size);
    }
    //强转然后解引用赋值,PREFIX_SIZE记录了这块内存的长度
    *((size_t*)ptr) = size;
    //返回的部分不包含这块内存的长度数据,所以做一个偏移
    return ((char*)ptr + ZMALLOC_PREFIX_SIZE);
}

//初始化分配的内存为0
void* zcalloc(size_t size)
{
    //calloc(cout, size)
    void *ptr = calloc(1, size + ZMALLOC_PREFIX_SIZE);
    if(!ptr)
    {
        zmalloc_oom_handler(size);
    }
    //强转然后解引用赋值,PREFIX_SIZE记录了这块内存的长度
    *((size_t*)ptr) = size;
    //返回的部分不包含这块内存的长度数据,所以做一个偏移
    return ((char*)ptr + ZMALLOC_PREFIX_SIZE);
}

//重新分配内存
void* zrealloc(void *ptr, size_t size)
{
    void* newptr;
    void* realptr;
    size_t oldsize;

    if(ptr == NULL)
    {
        return  zmalloc(size);
    }
    realptr = ((char*)ptr - ZMALLOC_PREFIX_SIZE);
    //强转解引用,获取分配的内存的大小
    oldsize = *((size_t*)realptr);
    newptr = realloc(realptr, size + ZMALLOC_PREFIX_SIZE);
    if(!newptr)
    {
        zmalloc_oom_handler(size);
    }

    *((size_t*)newptr) = size;
    return  ((char*)newptr + ZMALLOC_PREFIX_SIZE);
}

//释放内存
void  zfree(void* ptr)
{
    if(ptr == NULL)
    {
        return;
    }

    void* realptr;
    size_t oldsize;

    realptr = ((char*)ptr - ZMALLOC_PREFIX_SIZE);
    oldsize = *((size_t*)realptr);
    free(realptr);
}
//
// Created by Administrator on 2018/9/25.
//

#ifndef REDIS_BASIC_SDS_H
#define REDIS_BASIC_SDS_H

#include "memory.h"

typedef char *sds;
struct sdshdr
{
    //1.buf已占用长度
    size_t len;

    //2.buf剩余可用空间
    size_t free;

    //3.数据空间
    char buf[];
};
#define SDS_MAX_PREALLOC_SIZE (1024 * 1024)
#define SDS_HEADER_SIZE (sizeof(struct sdshdr))
#define SDS_TAIL_SYMBOL_SIZE (1)


//根据buf返回buf的长度
static inline size_t sdslen(const sds buf)
{
    struct sdshdr* sh = (void*)(buf - SDS_HEADER_SIZE);
    return sh->len;
}

//根据buf返回buf的剩余空间
static inline size_t sdsvail(const sds buf)
{
    struct sdshdr* sh = (void*)(buf - SDS_HEADER_SIZE);
    return sh->free;
}


//根据char指针,构造sds
sds sdsnew(const char* init);

//给定长度和初始数据,构造sds
sds sdsnewlen(const void* init, size_t initlen);

//拷贝sds的副本
sds sdsdup(const sds s);

//析构sds
void sdsfree(sds s);



#endif //REDIS_BASIC_SDS_H


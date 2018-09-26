//
// Created by Administrator on 2018/9/25.
//

#include "sds.h"


//根据char指针,构造sds
sds sdsnew(const char* init)
{
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return  sdsnewlen(init, initlen);
}


//给定长度和初始数据,构造sds
sds sdsnewlen(const void* init, size_t initlen)
{
    struct sdshdr *sh;

    if(init)
    {
        sh = zmalloc(SDS_HEADER_SIZE + initlen + SDS_TAIL_SYMBOL_SIZE);
    }
    else
    {
        sh = zcalloc(SDS_HEADER_SIZE + initlen + SDS_TAIL_SYMBOL_SIZE);
    }

    //内存分配失败
    if(sh == NULL)
    {
        return  NULL;
    }

    sh->len = initlen;
    //新的sds不预留空间
    sh->free = 0;

    if(initlen && init)
    {
        memcpy(sh->buf, init, initlen);
    }

    //以`\0`结尾,兼容部分C的函数
    sh->buf[initlen] = '\0';

    return (char*)sh->buf;
}

//拷贝sds的副本
sds sdsdup(const sds s)
{
    return sdsnewlen(s, sdslen(s));
}

//析构sds
void sdsfree(sds s)
{
    if(s == NULL)
    {
        return;
    }
    zfree(s - SDS_HEADER_SIZE);
}
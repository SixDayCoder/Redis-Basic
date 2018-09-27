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
    if(!sh)
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

//构造空sds
sds sdsempty()
{
    return sdsnewlen("", 0);
}

//拷贝sds的副本
sds sdsdup(const sds s)
{
    return sdsnewlen(s, sdslen(s));
}

//析构sds
void sdsfree(sds s)
{
    if(!s)
    {
        return;
    }
    zfree(s - SDS_HEADER_SIZE);
}

//sds的末尾扩充addlen个byte大小的内存
sds sdsMakeRoomFor(sds s, size_t addlen)
{
    size_t free = sdsvail(s);
    if(free >= addlen)
    {
        return  s;
    }

    struct sdshdr *sh = SDS_2_SDSHDR(s);
    size_t  oldlen = sh->len;
    size_t  newlen = (oldlen + addlen);

    //根据新长度确定要分配的内存的大小
    if(newlen < SDS_MAX_PREALLOC_SIZE)
    {
        newlen <<= 1;
    }
    else
    {
        newlen += SDS_MAX_PREALLOC_SIZE;
    }

    struct  sdshdr *nsh = zrealloc(sh, SDS_HEADER_SIZE + newlen + SDS_TAIL_SYMBOL_SIZE);
    if(nsh == NULL)
    {
        return NULL;
    }
    nsh->free = newlen - oldlen;
    return nsh->buf;
}

//填充s到指定长度,末尾以0填充
sds sdsgrowzero(sds s, size_t len)
{
    struct sdshdr *sh = SDS_2_SDSHDR(s);
    size_t currlen = sh->len;
    if(len <= currlen)
    {
        return s;
    }

    size_t appendsize = len - currlen;
    s = sdsMakeRoomFor(s, appendsize);
    if(s == NULL)
    {
        return  NULL;
    }

    sh = SDS_2_SDSHDR(s);
    size_t  totlen = sh->len + sh->free;
    //重置新加的内存空间为0,等同于
    //memset(s + currlen, 0, appendsize )
    //sh->buf[len] = '\0'
    memset(s + currlen, 0, appendsize + 1);
    sh->len = len;
    sh->free = totlen - len;
    return  s;
}
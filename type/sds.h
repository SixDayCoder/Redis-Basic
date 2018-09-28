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

#define SDS_LONG_INT_STR_SIZE (21)

#define SDS_SDSHDR(_s) ( (void*)(_s - SDS_HEADER_SIZE) )

#define SDS_ALLOC_SIZE(_s) do {\
    struct sdshdr *sh = SDS_SDSHDR(_s);\
    return sizeof(*sh) + sh->len + sh->free + 1;\
}while(0);

#define SDS_MAKE_SURE_HAVE_FREE(_s, _sh, _size) do {\
    if(_sh->free < _size)\
    {\
        _s = sdsMakeRoomFor(_s, _size);\
        _sh = SDS_SDSHDR(_s);\
    }\
}while(0);

#define SDS_REVERSE(_start, _end) do {\
    _end--;\
    char tmp;\
    while(_start < _end) {\
        tmp = *_start;\
        *_start = *_end;\
        *_end = tmp;\
        _start++;\
        _end--;\
    };\
}while(0);


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

//构造空sds
sds sdsempty();

//拷贝sds的副本
sds sdsdup(const sds s);

//析构sds
void sdsfree(sds s);

//sds的末尾扩充addlen个byte大小的内存
sds sdsMakeRoomFor(sds s, size_t addlen);

//填充s到指定长度,末尾以0填充
sds sdsgrowzero(sds s, size_t len);

//将长度为len的data数据添加到s的末尾
sds sdscatlen(sds s, const void* data, size_t len);

//将字符串str添加到s的末尾
sds sdscat(sds s, const char* str);

//将sds类型的data添加到s的末尾
sds sdscatsds(sds s, const sds data);

//复制字符串data的前len个字符替换s的内容
sds sdscpylen(sds s, const char* data, size_t len);

//复制字符串data替换s的内容
sds sdscpy(sds s, const char* data);

//longlong转字符串,返回该字符串的长度
size_t sdsll2str(char *s, long long value);

//unsignedlonglong转字符串,返回该字符串的长度
size_t sdsull2str(char *s, unsigned long long v);

//将要打印的内容copy到sds的末尾并返回
sds sdscatvprintf(sds s, const char *fmt, va_list ap);

//将要打印的内容copy到sds的末尾并返回
sds sdscatprintf(sds s, const char *fmt, ...);

//将要打印的内容copy到sds的末尾(很快)
sds sdscatfmt(sds s, char const *fmt, ...);

//sds两端trim掉cset中所有的字符
sds sdstrim(sds s, const char *cset);

//按照索引取sds字符串中的一段
void sdsrange(sds s, int start, int end);

//置s为空字符串,不释放内存
void sdsclear(sds s);

//lhs == rhs 返回0, lhs < rhs返回负数, lhs > rhs返回正数
int sdscmp(const sds lhs, const sds rhs);

//将s以分隔符sep划分,分成一个sds的数组存储在, count是该数组的长度
sds* sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);

//释放sds的数组
void sdslistfree(sds *sdslist, int count);

//字符全转为小写
void sdstolower(sds s);

//字符全转为大写
void sdstoupper(sds s);

//long long转sds
sds sdsfromlonglong(long long value);

//将s中的所有出现在from中的字符替换成to中的字符
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);

//s的剩余空间去掉incr,末尾补'\0'
void sdsIncrLen(sds s, int incr);

//回收s的剩余空间
sds sdsRemoveFreeSpace(sds s);


#endif //REDIS_BASIC_SDS_H

